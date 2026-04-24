#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "BarcodeScanner.h"
#include "DisplayManager.h"
#include "FoodAPI.h"
#include "Inventory.h"
#include "WebInterface.h"

// ============================================================
//  Zustands-Automat
// ============================================================
enum class State {
    BOOTING,
    WIFI_CONNECTING,
    IDLE,
    SCANNING,
    FETCHING,
    SHOW_PRODUCT,
    ENTER_DATE,
    SAVING,
    SUCCESS,
    ERROR,
    INVENTORY_BROWSE
};

// ============================================================
//  Objekte
// ============================================================
BarcodeScanner scanner(Serial2, BARCODE_RX_PIN, BARCODE_TX_PIN, BARCODE_BAUD);
DisplayManager display;
FoodAPI foodAPI;
Inventory inventory;
WebInterface* webInterface = nullptr;

// ============================================================
//  Taster (einfaches Debounce mit Flanken-Erkennung)
// ============================================================
class Button {
public:
    Button(uint8_t pin) : _pin(pin), _prev(HIGH), _lastChange(0) {}

    void begin() {
        pinMode(_pin, INPUT_PULLUP);
        _prev = HIGH;
    }

    // true genau einmal beim Drücken
    bool pressed() {
        bool cur = digitalRead(_pin);
        if (cur == LOW && _prev == HIGH && millis() - _lastChange > 60) {
            _lastChange = millis();
            _prev = cur;
            return true;
        }
        if (cur != _prev) {
            _lastChange = millis();
            _prev = cur;
        }
        return false;
    }

private:
    uint8_t _pin;
    bool _prev;
    unsigned long _lastChange;
};

Button btnUp(BTN_UP);
Button btnDown(BTN_DOWN);
Button btnOk(BTN_OK);

// ============================================================
//  Globale Zustands-Variablen
// ============================================================
State state = State::BOOTING;
ProductInfo currentProduct;
DateInput dateInput;
String currentBarcode;
String errorMsg;
int browseIndex = 0;

unsigned long stateEnter = 0;
unsigned long lastDisplayRefresh = 0;

void setState(State s) {
    state = s;
    stateEnter = millis();
}

// ============================================================
//  Hilfsfunktionen
// ============================================================
String todayStr() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return String(buf);
}

String dateInputToStr(const DateInput &d) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day);
    return String(buf);
}

String dateInputDisplay(const DateInput &d) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", d.day, d.month, d.year);
    return String(buf);
}

void initDateInput() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    dateInput.day   = t->tm_mday;
    dateInput.month = t->tm_mon + 1;
    dateInput.year  = t->tm_year + 1900;
    dateInput.activeField = FIELD_DAY;
}

int daysInMonth(int m, int y) {
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return days[m - 1];
}

void stepField(DateInput &d, int delta) {
    switch (d.activeField) {
        case FIELD_DAY:
            d.day += delta;
            if (d.day < 1)  d.day = daysInMonth(d.month, d.year);
            if (d.day > daysInMonth(d.month, d.year)) d.day = 1;
            break;
        case FIELD_MONTH:
            d.month += delta;
            if (d.month < 1)  d.month = 12;
            if (d.month > 12) d.month = 1;
            if (d.day > daysInMonth(d.month, d.year))
                d.day = daysInMonth(d.month, d.year);
            break;
        case FIELD_YEAR:
            d.year += delta;
            if (d.year < 2020) d.year = 2020;
            if (d.year > 2099) d.year = 2099;
            break;
        default: break;
    }
}

void advanceField(DateInput &d) {
    switch (d.activeField) {
        case FIELD_DAY:   d.activeField = FIELD_MONTH; break;
        case FIELD_MONTH: d.activeField = FIELD_YEAR;  break;
        case FIELD_YEAR:  d.activeField = FIELD_DONE;  break;
        default: break;
    }
}

int stockForBarcode(const String &barcode) {
    int total = 0;
    for (const auto &item : inventory.items()) {
        if (item.barcode == barcode) total += item.quantity;
    }
    return total;
}

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(100);

    if (!display.begin()) {
        // Display-Init fehlgeschlagen – nur seriell melden, weiter laufen
        Serial.println("[Display] Init fehlgeschlagen! Pins prüfen.");
    }
    display.showBooting("Display OK");
    delay(300);

    btnUp.begin();
    btnDown.begin();
    btnOk.begin();

    scanner.begin();
    display.showBooting("Scanner bereit");
    delay(300);

    // Inventar laden
    display.showBooting("Lade Inventar...");
    if (!inventory.begin()) {
        display.showError("LittleFS Fehler!");
        delay(3000);
    }

    // WiFi verbinden
    setState(State::WIFI_CONNECTING);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    bool up = btnUp.pressed();
    bool dn = btnDown.pressed();
    bool ok = btnOk.pressed();

    switch (state) {

    // ── WIFI_CONNECTING ──────────────────────────────────────
    case State::WIFI_CONNECTING: {
        static unsigned long lastDraw = 0;
        static int wifiAttempt = 0;
        if (millis() - lastDraw > 400) {
            display.showWifiConnecting(WIFI_SSID, ++wifiAttempt);
            lastDraw = millis();
        }
        if (WiFi.status() == WL_CONNECTED) {
            // NTP Zeit synchronisieren
            configTzTime(TZ_STRING, NTP_SERVER, NTP_SERVER2);
            Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

            // Web-Server starten
            webInterface = new WebInterface(inventory);
            webInterface->begin();

            setState(State::IDLE);
        }
        // Timeout nach 30 s → trotzdem weiter (kein Web / kein API)
        if (millis() - stateEnter > 30000) {
            Serial.println("[WiFi] Timeout – offline weiter");
            setState(State::IDLE);
        }
        break;
    }

    // ── IDLE ─────────────────────────────────────────────────
    case State::IDLE: {
        if (millis() - lastDisplayRefresh > 30000) {
            display.showIdle(inventory.count(),
                             inventory.expiringIn(WARNING_DAYS),
                             inventory.expiredCount());
            lastDisplayRefresh = millis();
        }
        if (stateEnter == millis() - (millis() - stateEnter)) {
            // Direkt beim Eintreten zeichnen
        }
        // Beim ersten Eintreten sofort zeichnen
        static State lastState = State::BOOTING;
        if (lastState != State::IDLE) {
            display.showIdle(inventory.count(),
                             inventory.expiringIn(WARNING_DAYS),
                             inventory.expiredCount());
            lastState = State::IDLE;
        }

        // Barcode gescannt → direkt weiter
        if (scanner.available()) {
            currentBarcode = scanner.getBarcode();
            Serial.printf("[Scan] Barcode: %s\n", currentBarcode.c_str());
            setState(State::FETCHING);
        }
        // OK-Taste → Inventar durchblättern
        if (ok && inventory.count() > 0) {
            browseIndex = 0;
            setState(State::INVENTORY_BROWSE);
        }
        break;
    }

    // ── SCANNING ─────────────────────────────────────────────
    case State::SCANNING: {
        static State lastState2 = State::BOOTING;
        if (lastState2 != State::SCANNING) {
            display.showScanning();
            lastState2 = State::SCANNING;
            scanner.flush();
        }
        if (scanner.available()) {
            currentBarcode = scanner.getBarcode();
            setState(State::FETCHING);
        }
        if (dn) setState(State::IDLE); // ZURÜCK
        break;
    }

    // ── FETCHING ─────────────────────────────────────────────
    case State::FETCHING: {
        static unsigned long lastDraw2 = 0;
        if (millis() - lastDraw2 > 500) {
            display.showFetching(currentBarcode);
            lastDraw2 = millis();
        }

        if (WiFi.status() == WL_CONNECTED) {
            currentProduct = foodAPI.lookup(currentBarcode);
        } else {
            currentProduct.found   = false;
            currentProduct.barcode = currentBarcode;
            currentProduct.name    = "Unbekannt (" + currentBarcode + ")";
        }

        initDateInput();
        setState(State::SHOW_PRODUCT);
        break;
    }

    // ── SHOW_PRODUCT ─────────────────────────────────────────
    case State::SHOW_PRODUCT: {
        static State lastSP = State::BOOTING;
        if (lastSP != State::SHOW_PRODUCT) {
            int stock = stockForBarcode(currentProduct.barcode);
            display.showProduct(currentProduct, stock);
            lastSP = State::SHOW_PRODUCT;
        }
        if (ok) {
            // Weiter zur Datum-Eingabe
            setState(State::ENTER_DATE);
        }
        if (dn) {
            // Zurück
            setState(State::IDLE);
            static State resetSP = State::BOOTING;
            lastSP = resetSP;
        }
        break;
    }

    // ── ENTER_DATE ───────────────────────────────────────────
    case State::ENTER_DATE: {
        static State lastED = State::BOOTING;
        if (lastED != State::ENTER_DATE) {
            display.showDateEntry(dateInput, currentProduct.name);
            lastED = State::ENTER_DATE;
        }

        bool redraw = false;
        if (up) { stepField(dateInput, +1); redraw = true; }
        if (dn) { stepField(dateInput, -1); redraw = true; }
        if (ok) {
            advanceField(dateInput);
            if (dateInput.activeField == FIELD_DONE) {
                setState(State::SAVING);
                break;
            }
            redraw = true;
        }
        if (redraw) {
            display.showDateEntry(dateInput, currentProduct.name);
        }
        break;
    }

    // ── SAVING ───────────────────────────────────────────────
    case State::SAVING: {
        InventoryItem item;
        item.barcode    = currentProduct.barcode;
        item.name       = currentProduct.name;
        item.brand      = currentProduct.brand;
        item.expiryDate = dateInputToStr(dateInput);
        item.addedDate  = todayStr();
        item.quantity   = 1;

        if (inventory.addItem(item)) {
            display.showSuccess(item.name, dateInputDisplay(dateInput));
            setState(State::SUCCESS);
        } else {
            errorMsg = "Speichern fehlgeschlagen";
            display.showError(errorMsg);
            setState(State::ERROR);
        }
        break;
    }

    // ── SUCCESS ──────────────────────────────────────────────
    case State::SUCCESS: {
        // Nach 2 s automatisch zurück, oder sofort mit Taste
        if (millis() - stateEnter > 2000 || ok || up || dn) {
            setState(State::IDLE);
            // IDLE-Anzeige sofort auffrischen
            lastDisplayRefresh = 0;
        }
        break;
    }

    // ── ERROR ────────────────────────────────────────────────
    case State::ERROR: {
        if (ok || millis() - stateEnter > 4000) {
            setState(State::IDLE);
        }
        break;
    }

    // ── INVENTORY_BROWSE ─────────────────────────────────────
    case State::INVENTORY_BROWSE: {
        const auto &items = inventory.items();
        if (items.empty()) {
            setState(State::IDLE);
            break;
        }

        static int lastIdx = -1;
        if (lastIdx != browseIndex) {
            const auto &it = items[browseIndex];

            // Tage bis Ablauf
            time_t now = time(nullptr);
            struct tm tm = {};
            tm.tm_year = it.expiryDate.substring(0, 4).toInt() - 1900;
            tm.tm_mon  = it.expiryDate.substring(5, 7).toInt() - 1;
            tm.tm_mday = it.expiryDate.substring(8, 10).toInt();
            int daysLeft = (int)(difftime(mktime(&tm), now) / 86400.0);

            display.showInventoryItem(browseIndex, items.size(),
                                      it.name, it.expiryDate,
                                      it.quantity, daysLeft);
            lastIdx = browseIndex;
        }

        if (up) {
            browseIndex = (browseIndex - 1 + items.size()) % items.size();
            lastIdx = -1;
        }
        if (dn) {
            browseIndex = (browseIndex + 1) % items.size();
            lastIdx = -1;
        }
        if (ok) {
            // Artikel löschen (1 Stück)
            const auto &it = items[browseIndex];
            inventory.removeItem(it.barcode, it.expiryDate);
            if (browseIndex >= (int)inventory.items().size())
                browseIndex = max(0, (int)inventory.items().size() - 1);
            lastIdx = -1;

            if (inventory.items().empty()) setState(State::IDLE);
        }
        // Langes Halten von AUF = zurück zur Idle
        static unsigned long upHold = 0;
        if (digitalRead(BTN_UP) == LOW) {
            if (upHold == 0) upHold = millis();
            if (millis() - upHold > 1500) {
                upHold = 0;
                lastIdx = -1;
                setState(State::IDLE);
            }
        } else {
            upHold = 0;
        }
        break;
    }

    } // switch
}
