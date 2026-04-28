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
#include "FoodAPI.h"
#include "Inventory.h"
#include "CustomProducts.h"
#include "Categories.h"
#include "WebInterface.h"

enum class State {
    BOOTING,
    WIFI_CONNECTING,
    AP_MODE,
    IDLE,
    SCANNING,
    FETCHING,
    SHOW_PRODUCT,
    CATEGORY_SELECT,   // Schritt 1: Kategorie wählen
    PRODUCT_LIST,      // Schritt 2: Produkt aus gefilterter Liste wählen
    ENTER_DATE,
    SAVING,
    SUCCESS,
    ERROR,
    INVENTORY_BROWSE
};

BarcodeScanner scanner(Serial1, BARCODE_RX_PIN, BARCODE_TX_PIN, BARCODE_BAUD);
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

State        state           = State::BOOTING;
ProductInfo  currentProduct;
DateInput    dateInput;
String       currentBarcode;
String       selectedCategory = "";
int          browseIndex      = 0;
int          listOffset       = 0;
unsigned long stateEnter      = 0;
bool         screenDirty      = true;

void setState(State s) { state = s; stateEnter = millis(); screenDirty = true; }

// ── WiFi-Konfiguration (NVS / Preferences) ───────────────────
// Nutzt NVS statt LittleFS → funktioniert ohne uploadfs-Flash

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
                  AP_SSID, AP_PASSWORD,
                  WiFi.softAPIP().toString().c_str());
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
int stockForBarcode(const String &bc) {
    int n=0; for (const auto &it : inventory.items()) if (it.barcode==bc) n+=it.quantity; return n;
}
int daysUntilExpiry(const String &ds) {
    if (ds.length()<10) return 9999;
    struct tm tm={};
    tm.tm_year=ds.substring(0,4).toInt()-1900;
    tm.tm_mon=ds.substring(5,7).toInt()-1;
    tm.tm_mday=ds.substring(8,10).toInt();
    return (int)(difftime(mktime(&tm),time(nullptr))/86400.0);
}

// ── setup() ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200); delay(100);

    Serial.printf("[PSRAM] Size: %u  Free: %u\n", ESP.getPsramSize(), ESP.getFreePsram());

    // Touch VOR Display initialisieren: beide teilen GPIO21 als RST.
    // touch.begin() pulst RST=LOW→HIGH und würde das Display zurücksetzen,
    // wenn display.begin() schon gelaufen ist.
#if BTN_BACK >= 0
    backBtn.begin();
#endif
    bool touchOk = touch.begin();
    if (!touchOk) Serial.println("[Touch] CST816S nicht gefunden!");

    bool dispOk = display.begin();
    Serial.printf("[Display] begin()=%d\n", dispOk);
    if (!dispOk) Serial.println("[Display] Init fehlgeschlagen!");
    display.showBooting("Display + Touch OK"); delay(300);

    scanner.begin();
    display.showBooting("Scanner OK"); delay(200);

    display.showBooting("Lade Daten...");
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed – kein persistenter Speicher");
        display.showBooting("FS-Fehler!");
        delay(1000);
    } else {
        Serial.println("[FS] LittleFS OK");
        if (!inventory.begin()) Serial.println("[Inventory] Fehler!");
        customProducts.begin();
    }

    setState(State::WIFI_CONNECTING);
    auto wCfg = loadWifiCfg();
    if (wCfg.ssid.isEmpty()) {
        startAP(); // Keine Zugangsdaten → sofort AP
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

    auto hit = [&](int16_t bx, int16_t by, int16_t bw, int16_t bh) {
        return tapped && tx>=bx && tx<bx+bw && ty>=by && ty<by+bh;
    };

    bool hardBack = false;
#if BTN_BACK >= 0
    hardBack = backBtn.pressed();
#endif

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
            setState(State::IDLE);
        }
        if (millis()-stateEnter > 20000) { startAP(); }
        break;
    }

    // ── AP_MODE ──────────────────────────────────────────────
    case State::AP_MODE: {
        if (screenDirty) {
            display.showAPMode(AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString());
            screenDirty = false;
        }
        if (scanner.available()) {
            currentBarcode = scanner.getBarcode();
            setState(State::FETCHING);
        }
        if (hit(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H) && customProducts.count()>0)
            setState(State::CATEGORY_SELECT);
        if (hit(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H) && inventory.count()>0) {
            browseIndex=0; setState(State::INVENTORY_BROWSE);
        }
        break;
    }

    // ── IDLE ─────────────────────────────────────────────────
    case State::IDLE: {
        if (screenDirty) {
            display.showIdle(inventory.count(), inventory.expiringIn(WARNING_DAYS),
                             inventory.expiredCount(), customProducts.count());
            screenDirty = false;
        }
        if (scanner.available()) {
            currentBarcode = scanner.getBarcode();
            Serial.printf("[Scan] %s\n", currentBarcode.c_str());
            setState(State::FETCHING); break;
        }
        if (hit(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H) && customProducts.count()>0)
            setState(State::CATEGORY_SELECT);
        if (hit(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H) && inventory.count()>0) {
            browseIndex=0; setState(State::INVENTORY_BROWSE);
        }
        break;
    }

    // ── SCANNING ─────────────────────────────────────────────
    case State::SCANNING: {
        if (screenDirty) { display.showScanning(); screenDirty=false; }
        if (scanner.available()) { currentBarcode=scanner.getBarcode(); setState(State::FETCHING); }
        if (hit(TBTN_X,TBTN_PRIMARY_Y,TBTN_W,TBTN_H) || hardBack ||
            gest==Gesture::SWIPE_LEFT || gest==Gesture::SWIPE_RIGHT)
        { scanner.flush(); setState(State::IDLE); }
        break;
    }

    // ── FETCHING ─────────────────────────────────────────────
    case State::FETCHING: {
        static unsigned long lastAnim=0;
        if (millis()-lastAnim > 500) { display.showFetching(currentBarcode); lastAnim=millis(); }
        currentProduct = (WiFi.status()==WL_CONNECTED)
            ? foodAPI.lookup(currentBarcode)
            : ProductInfo{false,currentBarcode,"Unbekannt ("+currentBarcode+")"};
        initDateToday();
        setState(State::SHOW_PRODUCT);
        break;
    }

    // ── SHOW_PRODUCT ─────────────────────────────────────────
    case State::SHOW_PRODUCT: {
        if (screenDirty) {
            display.showProduct(currentProduct, stockForBarcode(currentProduct.barcode));
            screenDirty=false;
        }
        if (hit(TBTN_X,TBTN_PRIMARY_Y,TBTN_W,TBTN_H))   setState(State::ENTER_DATE);
        if (hit(TBTN_X,TBTN_SECONDARY_Y,TBTN_W,TBTN_H) || hardBack ||
            gest==Gesture::SWIPE_LEFT || gest==Gesture::SWIPE_RIGHT)
            setState(State::IDLE);
        break;
    }

    // ── CATEGORY_SELECT ──────────────────────────────────────
    case State::CATEGORY_SELECT: {
        if (screenDirty) { display.showCategorySelect(); screenDirty=false; }

        // Tap in das 4×2 Kachel-Grid (Landscape)
        if (tapped && ty >= CAT_Y0 && ty < CAT_Y0 + 2*(CAT_BTN_H+CAT_GAP)) {
            int col = (tx - CAT_X0) / (CAT_BTN_W + CAT_GAP);
            int row = (ty - CAT_Y0) / (CAT_BTN_H + CAT_GAP);
            if (col>=0 && col<4 && row>=0 && row<2) {
                int16_t bx = CAT_X0 + col*(CAT_BTN_W+CAT_GAP);
                int16_t by = CAT_Y0 + row*(CAT_BTN_H+CAT_GAP);
                if (tx>=bx && tx<bx+CAT_BTN_W && ty>=by && ty<by+CAT_BTN_H) {
                    selectedCategory = CATEGORIES[row*4+col].name;
                    listOffset = 0;
                    setState(State::PRODUCT_LIST);
                }
            }
        }
        if (hardBack || gest==Gesture::SWIPE_RIGHT || gest==Gesture::SWIPE_LEFT)
            setState(State::IDLE);
        break;
    }

    // ── PRODUCT_LIST ─────────────────────────────────────────
    case State::PRODUCT_LIST: {
        // Gefilterte Liste nur bei Bedarf neu aufbauen
        static std::vector<CustomProduct> filtered;
        if (screenDirty) {
            filtered = customProducts.byCategory(selectedCategory);
            display.showProductList(filtered, listOffset, selectedCategory);
            screenDirty = false;
        }

        // Tap auf einen Listen-Eintrag
        if (tapped && ty >= LIST_ITEMS_Y && ty < LIST_ITEMS_Y + LIST_MAX_VIS*LIST_ITEM_H) {
            int idx = listOffset + (ty-LIST_ITEMS_Y)/LIST_ITEM_H;
            if (idx < (int)filtered.size()) {
                const auto &p = filtered[idx];
                currentProduct = { true, p.barcode, p.name, p.brand, "", "" };
                currentBarcode = p.barcode;
                initDateToday();
                setState(State::ENTER_DATE); break;
            }
        }
        // Wischen zum Scrollen
        if (gest==Gesture::SWIPE_UP || gest==Gesture::SWIPE_LEFT) {
            int maxOff = max(0,(int)filtered.size()-LIST_MAX_VIS);
            if (listOffset<maxOff) { listOffset++; screenDirty=true; }
        }
        if (gest==Gesture::SWIPE_DOWN || gest==Gesture::SWIPE_RIGHT) {
            if (listOffset>0) { listOffset--; screenDirty=true; }
        }
        if (hit(TBTN_X,LIST_BACK_BTN_Y,TBTN_W,40) || hardBack)
            setState(State::CATEGORY_SELECT);
        break;
    }

    // ── ENTER_DATE ───────────────────────────────────────────
    case State::ENTER_DATE: {
        if (screenDirty) { display.showDateEntry(dateInput,currentProduct.name); screenDirty=false; }
        bool changed=false;
        if (tapped && ty>=DATE_PLUS_Y0 && ty<DATE_PLUS_Y1) {
            int col=tx/DATE_COL_W;
            if      (col==0){dateInput.day++;   clampDate(dateInput);changed=true;}
            else if (col==1){dateInput.month++;  clampDate(dateInput);changed=true;}
            else if (col==2){dateInput.year++;   clampDate(dateInput);changed=true;}
        }
        if (tapped && ty>=DATE_MINUS_Y0 && ty<DATE_MINUS_Y1) {
            int col=tx/DATE_COL_W;
            if      (col==0){dateInput.day--;   clampDate(dateInput);changed=true;}
            else if (col==1){dateInput.month--;  clampDate(dateInput);changed=true;}
            else if (col==2){dateInput.year--;   clampDate(dateInput);changed=true;}
        }
        if (changed) display.showDateEntry(dateInput,currentProduct.name);
        if (hit(DATE_OK_X, DATE_BTN_Y, DATE_OK_W, DATE_BTN_H)) setState(State::SAVING);
        if (hit(DATE_BACK_X, DATE_BTN_Y, DATE_BACK_W, DATE_BTN_H) || hardBack) {
            setState(selectedCategory.isEmpty() ? State::SHOW_PRODUCT : State::PRODUCT_LIST);
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
            display.showError("Speichern fehlgeschlagen!");
            setState(State::ERROR);
        }
        break;
    }

    // ── SUCCESS ──────────────────────────────────────────────
    case State::SUCCESS:
        if (millis()-stateEnter>2500 || tapped || hardBack) setState(State::IDLE);
        break;

    // ── ERROR ────────────────────────────────────────────────
    case State::ERROR:
        if (tapped || hardBack || millis()-stateEnter>5000) setState(State::IDLE);
        break;

    // ── INVENTORY_BROWSE ─────────────────────────────────────
    case State::INVENTORY_BROWSE: {
        const auto &items = inventory.items();
        if (items.empty()) { setState(State::IDLE); break; }
        if (browseIndex>=(int)items.size()) browseIndex=items.size()-1;
        if (screenDirty) {
            const auto &it=items[browseIndex];
            display.showInventoryItem(browseIndex,items.size(),
                                      it.name,it.expiryDate,
                                      it.quantity,daysUntilExpiry(it.expiryDate));
            screenDirty=false;
        }
        if (gest==Gesture::SWIPE_UP   || gest==Gesture::SWIPE_LEFT)
            { browseIndex=(browseIndex+1)%items.size(); screenDirty=true; }
        if (gest==Gesture::SWIPE_DOWN || gest==Gesture::SWIPE_RIGHT)
            { browseIndex=(browseIndex-1+items.size())%items.size(); screenDirty=true; }
        if (hit(TBTN_X,INV_DEL_Y,TBTN_W,TBTN_H)) {
            inventory.removeItem(items[browseIndex].barcode,items[browseIndex].expiryDate);
            browseIndex=min(browseIndex,max(0,(int)inventory.items().size()-1));
            if (inventory.items().empty()) setState(State::IDLE); else screenDirty=true;
        }
        if (hit(TBTN_X,INV_BACK_Y,TBTN_W,TBTN_H) || hardBack || gest==Gesture::LONG_PRESS)
            setState(State::IDLE);
        break;
    }

    } // switch
}
