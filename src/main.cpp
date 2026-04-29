#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "BarcodeScanner.h"
#include "DisplayManager.h"
#include "TouchController.h"
#include "ThermalPrinter.h"
#include "FoodAPI.h"
#include "Inventory.h"
#include "CustomProducts.h"
#include "Categories.h"
#include "CategoryManager.h"
#include "WebInterface.h"

// ── States ────────────────────────────────────────────────────

enum class State {
    BOOTING,
    WIFI_CONNECTING,
    AP_MODE,
    MAIN,             // Hauptscreen: Kategorietabs + Produktliste
    FETCHING,         // Barcode-Lookup (Open Food Facts)
    ENTER_DATE,       // Haltbarkeitsdatum eingeben
    SAVING,           // In Inventar speichern + Label-Code generieren
    PRINTING,         // Etikett drucken
    SUCCESS,          // Eingelagert-Bestätigung
    ERROR,
    RETRIEVE,         // Label-Barcode gescannt → Auslagerungs-Bestätigung
    INVENTORY_BROWSE, // Inventar durchblättern
    POWER_SAVE,       // Display aus, Scanner schläft
};

// ── Globale Objekte ───────────────────────────────────────────

BarcodeScanner scanner(Serial1, BARCODE_RX_PIN, BARCODE_TX_PIN, BARCODE_BAUD);
ThermalPrinter printer(Serial2, PRINTER_TX_PIN, PRINTER_RX_PIN, PRINTER_BAUD);
DisplayManager display;
TouchController touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);
FoodAPI        foodAPI;
Inventory      inventory;
CustomProducts customProducts;
WebInterface  *webInterface = nullptr;

#if BTN_BACK >= 0
class BackButton {
    uint8_t _pin; bool _prev = HIGH; unsigned long _last = 0;
public:
    BackButton(uint8_t p) : _pin(p) {}
    void begin() { pinMode(_pin, INPUT_PULLUP); }
    bool pressed() {
        bool cur = digitalRead(_pin);
        if (cur == LOW && _prev == HIGH && millis() - _last > 80) {
            _last = millis(); _prev = cur; return true;
        }
        if (cur != _prev) { _last = millis(); _prev = cur; }
        return false;
    }
} backBtn(BTN_BACK);
#endif

// ── Zustandsvariablen ─────────────────────────────────────────

State         state          = State::BOOTING;
ProductInfo   currentProduct;
DateInput     dateInput;
String        currentBarcode;
InventoryItem retrieveItem;   // Artikel bei Auslagerungs-Scan
InventoryItem lastAddedItem;  // Für Drucker nach SAVING
int           currentCatIndex = 0;  // Aktive Kategorie im Hauptscreen
int           mainOffset      = 0;  // Scroll-Offset Hauptliste
int           browseIndex     = 0;
unsigned long stateEnter      = 0;
bool          screenDirty     = true;
unsigned long lastActivity    = 0;

void setState(State s) { state = s; stateEnter = millis(); screenDirty = true; }

// ── WiFi-Konfiguration (NVS) ──────────────────────────────────

struct WifiCfg { String ssid, password; };

WifiCfg loadWifiCfg() {
    Preferences prefs;
    prefs.begin("wifi", true);
    String s = prefs.getString("ssid",     WIFI_SSID);
    String p = prefs.getString("password", WIFI_PASSWORD);
    prefs.end();
    return { s, p };
}

void saveWifiCfg(const String &ssid, const String &password) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid",     ssid);
    prefs.putString("password", password);
    prefs.end();
}

void startAP() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WiFi] AP: %s  PW: %s  IP: %s\n",
                  AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    webInterface = new WebInterface(inventory, customProducts);
    webInterface->begin();
    setState(State::AP_MODE);
}

// ── Hilfs-Funktionen ──────────────────────────────────────────

String todayStr() {
    time_t now = time(nullptr); struct tm *t = localtime(&now);
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday);
    return buf;
}

// "YYYY-MM-DD" → "DD.MM.YYYY" für Anzeige und Etikett
String isoToDisplay(const String &iso) {
    if (iso.length() < 10) return iso;
    return iso.substring(8,10) + "." + iso.substring(5,7) + "." + iso.substring(0,4);
}

String dateInputToStr(const DateInput &d) {
    char buf[12]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day); return buf;
}
String dateInputDisplay(const DateInput &d) {
    char buf[12]; snprintf(buf, sizeof(buf), "%02d.%02d.%04d", d.day, d.month, d.year); return buf;
}

void initDateToday() {
    time_t now = time(nullptr); struct tm *t = localtime(&now);
    dateInput = { t->tm_mday, t->tm_mon+1, t->tm_year+1900, FIELD_DAY };
}

int daysInMonth(int m, int y) {
    const int d[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if (m==2 && ((y%4==0&&y%100!=0)||y%400==0)) return 29;
    return d[m-1];
}

void clampDate(DateInput &d) {
    if (d.month<1)  d.month=12; if (d.month>12) d.month=1;
    if (d.day<1)    d.day=daysInMonth(d.month,d.year);
    if (d.day>daysInMonth(d.month,d.year)) d.day=1;
    if (d.year<2024) d.year=2024; if (d.year>2099) d.year=2099;
}

int daysUntilExpiry(const String &ds) {
    if (ds.length()<10) return 9999;
    struct tm tm={};
    tm.tm_year=ds.substring(0,4).toInt()-1900;
    tm.tm_mon=ds.substring(5,7).toInt()-1;
    tm.tm_mday=ds.substring(8,10).toInt();
    return (int)(difftime(mktime(&tm),time(nullptr))/86400.0);
}

// Erzeugt eindeutigen Etikett-Barcode "L00001" aus NVS-Zähler
String generateLabelCode() {
    Preferences prefs;
    prefs.begin("lager", false);
    uint32_t n = prefs.getUInt("cnt", 0) + 1;
    prefs.putUInt("cnt", n);
    prefs.end();
    char buf[10];
    snprintf(buf, sizeof(buf), "L%05u", n);
    return buf;
}

// ── setup() ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200); delay(100);
    Serial.printf("[PSRAM] Size: %u  Free: %u\n", ESP.getPsramSize(), ESP.getFreePsram());

#if BTN_BACK >= 0
    backBtn.begin();
#endif

    // Touch VOR Display (teilen GPIO21 als RST)
    bool touchOk = touch.begin();
    if (!touchOk) Serial.println("[Touch] CST816S nicht gefunden!");

    bool dispOk = display.begin();
    Serial.printf("[Display] begin()=%d\n", dispOk);
    display.showBooting("Display + Touch OK"); delay(300);

    scanner.begin();
    display.showBooting("Scanner OK"); delay(200);

    printer.begin();
    display.showBooting("Drucker OK"); delay(200);

    display.showBooting("Lade Daten...");
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed");
        display.showBooting("FS-Fehler!"); delay(1000);
    } else {
        Serial.println("[FS] LittleFS OK");
        categoryManager.begin();
        if (!inventory.begin())  Serial.println("[Inventory] Fehler!");
        customProducts.begin();
    }

    lastActivity = millis();
    setState(State::WIFI_CONNECTING);
    auto wCfg = loadWifiCfg();
    if (wCfg.ssid.isEmpty()) {
        startAP();
    } else {
        WiFi.setHostname(HOSTNAME);
        WiFi.begin(wCfg.ssid.c_str(), wCfg.password.c_str());
    }
}

// ── loop() ────────────────────────────────────────────────────

void loop() {
    touch.update();
    bool    tapped = touch.wasTapped();
    int16_t tx     = touch.tapX();
    int16_t ty     = touch.tapY();
    Gesture gest   = touch.lastGesture();

    if (tapped) lastActivity = millis();

    auto hit = [&](int16_t bx, int16_t by, int16_t bw, int16_t bh) {
        return tapped && tx>=bx && tx<bx+bw && ty>=by && ty<by+bh;
    };

    bool hardBack = false;
#if BTN_BACK >= 0
    hardBack = backBtn.pressed();
    if (hardBack) lastActivity = millis();
#endif

    // ── Barcode-Scanner: immer aktiv ─────────────────────────
    if (scanner.available()) {
        String bc = scanner.getBarcode();
        lastActivity = millis();
        Serial.printf("[Scan] %s\n", bc.c_str());

        if (state == State::POWER_SAVE) {
            // Aufwecken, Barcode wird nach Rückkehr erneut gescannt
            display.setBrightness(220);
            setState(State::MAIN);
        } else {
            // Label-Barcode (L00001 … L99999) → Auslagerung
            bool isLabel = (bc.length() == 6 && bc[0] == 'L');
            const InventoryItem *found = isLabel ? inventory.findByLabel(bc) : nullptr;
            if (found) {
                retrieveItem = *found;
                setState(State::RETRIEVE);
            } else {
                // Normaler Produkt-Barcode → Online-Suche
                currentBarcode = bc;
                setState(State::FETCHING);
            }
        }
        return; // Verarbeitung in nächstem loop()-Aufruf
    }

    // ── Power-Save-Prüfung ───────────────────────────────────
    if (state != State::POWER_SAVE &&
        state != State::BOOTING   &&
        state != State::WIFI_CONNECTING &&
        millis() - lastActivity > POWER_SAVE_MS) {
        display.setBrightness(0);
        setState(State::POWER_SAVE);
    }

    // ── State-Machine ─────────────────────────────────────────
    switch (state) {

    // ── WIFI_CONNECTING ──────────────────────────────────────
    case State::WIFI_CONNECTING: {
        static unsigned long lastDraw = 0; static int attempt = 0;
        static String connectSsid = WiFi.SSID();
        if (millis()-lastDraw > 450) {
            display.showWifiConnecting(connectSsid.isEmpty() ? "..." : connectSsid, ++attempt);
            lastDraw = millis();
        }
        if (WiFi.status()==WL_CONNECTED) {
            configTzTime(TZ_STRING, NTP_SERVER, NTP_SERVER2);
            Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            webInterface = new WebInterface(inventory, customProducts);
            webInterface->begin();
            setState(State::MAIN);
        }
        if (millis()-stateEnter > 20000) startAP();
        break;
    }

    // ── AP_MODE ──────────────────────────────────────────────
    case State::AP_MODE: {
        if (screenDirty) {
            display.showAPMode(AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString());
            screenDirty = false;
        }
        // Zur Produktliste wechseln
        if (hit(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H))
            setState(State::MAIN);
        if (hit(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H) && inventory.count()>0) {
            browseIndex=0; setState(State::INVENTORY_BROWSE);
        }
        break;
    }

    // ── MAIN ─────────────────────────────────────────────────
    case State::MAIN: {
        static std::vector<CustomProduct> mainProducts;
        if (screenDirty) {
            String catName = (currentCatIndex < (int)g_categories.size())
                             ? g_categories[currentCatIndex].name : "";
            mainProducts = customProducts.byCategory(catName);
            display.showMain(currentCatIndex, mainProducts, mainOffset);
            screenDirty = false;
        }
        // Tab-Tap (obere Leiste) – dynamische Breite
        if (tapped && ty < TABS_H) {
            int nCats  = max(1, (int)g_categories.size());
            int newCat = tx / (DISPLAY_W / nCats);
            if (newCat >= 0 && newCat < nCats && newCat != currentCatIndex) {
                currentCatIndex = newCat;
                mainOffset      = 0;
                screenDirty     = true;
            }
            break;
        }
        // Produkt antippen → Datumseingabe (mit defaultDays Auto-MHD)
        int relY = ty - MAIN_LIST_Y;
        if (tapped && relY >= 0 && relY < MAIN_MAX_VIS * MAIN_ITEM_H) {
            int idx = mainOffset + relY / MAIN_ITEM_H;
            if (idx < (int)mainProducts.size()) {
                const auto &p = mainProducts[idx];
                currentProduct = { true, p.barcode, p.name, p.brand, "", "" };
                currentBarcode = p.barcode;
                if (p.defaultDays > 0) {
                    // MHD automatisch = heute + defaultDays
                    time_t future = time(nullptr) + (time_t)p.defaultDays * 86400;
                    struct tm *ft = localtime(&future);
                    dateInput = { ft->tm_mday, ft->tm_mon+1, ft->tm_year+1900, FIELD_DAY };
                } else {
                    initDateToday();
                }
                setState(State::ENTER_DATE);
                break;
            }
        }
        // "Lager"-Button (unten rechts)
        if (tapped && ty >= MAIN_HINT_Y && tx >= MAIN_INV_X && inventory.count()>0) {
            browseIndex=0; setState(State::INVENTORY_BROWSE); break;
        }
        // Scrollen
        if (gest==Gesture::SWIPE_UP || gest==Gesture::SWIPE_LEFT) {
            int maxOff = max(0,(int)mainProducts.size()-MAIN_MAX_VIS);
            if (mainOffset<maxOff) { mainOffset++; screenDirty=true; }
        }
        if (gest==Gesture::SWIPE_DOWN || gest==Gesture::SWIPE_RIGHT) {
            if (mainOffset>0) { mainOffset--; screenDirty=true; }
        }
        break;
    }

    // ── FETCHING ─────────────────────────────────────────────
    case State::FETCHING: {
        static unsigned long lastAnim=0;
        if (millis()-lastAnim > 500) { display.showFetching(currentBarcode); lastAnim=millis(); }
        currentProduct = (WiFi.status()==WL_CONNECTED)
            ? foodAPI.lookup(currentBarcode)
            : ProductInfo{false, currentBarcode, "Unbekannt ("+currentBarcode+")"};
        initDateToday();
        setState(State::ENTER_DATE);
        break;
    }

    // ── ENTER_DATE ───────────────────────────────────────────
    case State::ENTER_DATE: {
        if (screenDirty) { display.showDateEntry(dateInput, currentProduct.name); screenDirty=false; }
        bool changed=false;
        if (tapped && ty>=DATE_PLUS_Y0 && ty<DATE_PLUS_Y1) {
            int col=tx/DATE_COL_W;
            if      (col==0){dateInput.day++;   clampDate(dateInput);changed=true;}
            else if (col==1){dateInput.month++; clampDate(dateInput);changed=true;}
            else if (col==2){dateInput.year++;  clampDate(dateInput);changed=true;}
        }
        if (tapped && ty>=DATE_MINUS_Y0 && ty<DATE_MINUS_Y1) {
            int col=tx/DATE_COL_W;
            if      (col==0){dateInput.day--;   clampDate(dateInput);changed=true;}
            else if (col==1){dateInput.month--; clampDate(dateInput);changed=true;}
            else if (col==2){dateInput.year--;  clampDate(dateInput);changed=true;}
        }
        if (changed) display.showDateEntry(dateInput, currentProduct.name);
        if (hit(DATE_OK_X, DATE_BTN_Y, DATE_OK_W, DATE_BTN_H))
            setState(State::SAVING);
        if (hit(DATE_BACK_X, DATE_BTN_Y, DATE_BACK_W, DATE_BTN_H) || hardBack)
            setState(State::MAIN);
        break;
    }

    // ── SAVING ───────────────────────────────────────────────
    case State::SAVING: {
        String labelCode = generateLabelCode();
        InventoryItem item;
        item.barcode      = currentBarcode;
        item.name         = currentProduct.name;
        item.brand        = currentProduct.brand;
        item.expiryDate   = dateInputToStr(dateInput);
        item.addedDate    = todayStr();
        item.quantity     = 1;
        item.labelBarcode = labelCode;
        if (inventory.addItem(item)) {
            lastAddedItem = item;
            setState(State::PRINTING);
        } else {
            display.showError("Speichern fehlgeschlagen!");
            setState(State::ERROR);
        }
        break;
    }

    // ── PRINTING ─────────────────────────────────────────────
    case State::PRINTING: {
        if (screenDirty) {
            display.showPrinting();
            screenDirty = false;
            // Etikett drucken (blockierend ~1-2s)
            printer.printLabel(
                lastAddedItem.name,
                lastAddedItem.labelBarcode,
                isoToDisplay(lastAddedItem.addedDate),
                lastAddedItem.expiryDate.isEmpty() ? "" : isoToDisplay(lastAddedItem.expiryDate)
            );
        }
        // Nach Druck kurz Erfolgsmeldung, dann zurück
        if (millis()-stateEnter > 2200) {
            display.showSuccess(lastAddedItem.name, dateInputDisplay(dateInput));
            setState(State::SUCCESS);
        }
        break;
    }

    // ── SUCCESS ──────────────────────────────────────────────
    case State::SUCCESS:
        if (millis()-stateEnter>2000 || tapped || hardBack) setState(State::MAIN);
        break;

    // ── ERROR ────────────────────────────────────────────────
    case State::ERROR:
        if (tapped || hardBack || millis()-stateEnter>5000) setState(State::MAIN);
        break;

    // ── RETRIEVE ─────────────────────────────────────────────
    case State::RETRIEVE: {
        if (screenDirty) {
            int days = daysUntilExpiry(retrieveItem.expiryDate);
            display.showRetrieve(retrieveItem.name,
                                  isoToDisplay(retrieveItem.addedDate),
                                  isoToDisplay(retrieveItem.expiryDate),
                                  days);
            screenDirty = false;
        }
        if (hit(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H)) {
            // Auslagern: aus Inventar entfernen
            inventory.removeByLabel(retrieveItem.labelBarcode);
            setState(State::MAIN);
        }
        if (hit(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, TBTN_H) || hardBack)
            setState(State::MAIN);
        break;
    }

    // ── INVENTORY_BROWSE ─────────────────────────────────────
    case State::INVENTORY_BROWSE: {
        const auto &items = inventory.items();
        if (items.empty()) { setState(State::MAIN); break; }
        if (browseIndex>=(int)items.size()) browseIndex=items.size()-1;
        if (screenDirty) {
            const auto &it=items[browseIndex];
            display.showInventoryItem(browseIndex, items.size(),
                                      it.name, it.expiryDate,
                                      it.quantity, daysUntilExpiry(it.expiryDate));
            screenDirty=false;
        }
        if (gest==Gesture::SWIPE_UP   || gest==Gesture::SWIPE_LEFT)
            { browseIndex=(browseIndex+1)%items.size(); screenDirty=true; }
        if (gest==Gesture::SWIPE_DOWN || gest==Gesture::SWIPE_RIGHT)
            { browseIndex=(browseIndex-1+items.size())%items.size(); screenDirty=true; }
        if (hit(TBTN_X, INV_DEL_Y, TBTN_W, TBTN_H)) {
            inventory.removeItem(items[browseIndex].barcode, items[browseIndex].expiryDate);
            browseIndex=min(browseIndex, max(0,(int)inventory.items().size()-1));
            if (inventory.items().empty()) setState(State::MAIN); else screenDirty=true;
        }
        if (hit(TBTN_X, INV_BACK_Y, TBTN_W, TBTN_H) || hardBack || gest==Gesture::LONG_PRESS)
            setState(State::MAIN);
        break;
    }

    // ── POWER_SAVE ───────────────────────────────────────────
    case State::POWER_SAVE: {
        if (screenDirty) screenDirty = false;  // Display bleibt schwarz
        if (tapped) {
            lastActivity = millis();
            display.setBrightness(220);
            setState(State::MAIN);
        }
        break;
    }

    case State::BOOTING: break;

    } // switch
}
