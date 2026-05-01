#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>
#include "config.h"
#include "BarcodeScanner.h"
#include "DisplayManager.h"
#include "TouchController.h"
#include "ThermalPrinter.h"
#include "FoodAPI.h"
#include "FoodCache.h"
#include "Inventory.h"
#include "CustomProducts.h"
#include "Categories.h"
#include "CategoryManager.h"
#include "StorageStats.h"
#include "ShoppingList.h"
#include "ServerSync.h"
#include "WebInterface.h"
#include <HTTPClient.h>

// ── States ────────────────────────────────────────────────────

enum class State {
    BOOTING,
    WIFI_CONNECTING,
    AP_MODE,
    MAIN,             // Hauptscreen: Kategorie-Grid
    PRODUCT_LIST,     // Produktliste einer Kategorie
    FETCHING,         // Barcode-Lookup (Open Food Facts)
    ENTER_DATE,       // Haltbarkeitsdatum eingeben (Numpad)
    SAVING,           // In Inventar speichern + Label-Code generieren
    PRINTING,         // Etikett drucken
    SUCCESS,          // Eingelagert-Bestätigung
    ERROR,
    RETRIEVE,         // Label-Barcode gescannt → Auslagerungs-Bestätigung
    INVENTORY_BROWSE, // Inventar durchblättern
    POWER_SAVE,       // Display aus, Scanner schläft
};

// ── Globale Objekte ───────────────────────────────────────────

HardwareSerial scannerSerial(0);  // UART0 = native GPIO44(RX) / GPIO43(TX)
BarcodeScanner scanner(scannerSerial, BARCODE_RX_PIN, BARCODE_TX_PIN, BARCODE_BAUD);
ThermalPrinter printer(Serial1, PRINTER_TX_PIN, PRINTER_RX_PIN, PRINTER_BAUD);
DisplayManager display;
TouchController touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);
FoodAPI        foodAPI;
Inventory      inventory;
CustomProducts customProducts;
WebInterface  *webInterface = nullptr;

// ── Scan-Log (RAM-basiert, letzte ~100 Scans) ─────────────────

struct ScanLogEntry { unsigned long ts; String barcode; };
static const int MAX_SCAN_LOGS = 100;
static ScanLogEntry scanLogs[MAX_SCAN_LOGS];
static int scanLogIndex = 0;

void logScan(const String &barcode) {
    scanLogs[scanLogIndex] = { millis() / 1000, barcode };
    scanLogIndex = (scanLogIndex + 1) % MAX_SCAN_LOGS;
}

String getScanLogsJSON() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_SCAN_LOGS; i++) {
        int idx = (scanLogIndex + i) % MAX_SCAN_LOGS;
        if (scanLogs[idx].barcode.length() > 0) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ts"] = scanLogs[idx].ts;
            obj["barcode"] = scanLogs[idx].barcode;
        }
    }
    String out; serializeJson(doc, out);
    return out;
}

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
bool          g_useNumpad     = false;  // Datumseingabe-Modus (geladen aus NVS)

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
    webInterface = new WebInterface(inventory, customProducts, storageStats, shoppingList);
    webInterface->begin();
    setState(State::AP_MODE);
}

// ── MQTT ──────────────────────────────────────────────────────

struct MqttCfg { String host; uint16_t port = 1883; String prefix = "lebensmittel"; };

WiFiClient        _mqttWifi;
PubSubClient      _mqtt(_mqttWifi);
static MqttCfg    _mqttCfg;
static unsigned long _mqttLastCheck = 0;

MqttCfg loadMqttCfg() {
    MqttCfg c;
    Preferences p; p.begin("mqtt", true);
    c.host   = p.getString("host",   "");
    c.port   = p.getUShort("port",   1883);
    c.prefix = p.getString("prefix", "lebensmittel");
    p.end();
    return c;
}
void saveMqttCfg(const MqttCfg &c) {
    Preferences p; p.begin("mqtt", false);
    p.putString("host",   c.host);
    p.putUShort("port",   c.port);
    p.putString("prefix", c.prefix);
    p.end();
}
void mqttEnsureConnected() {
    if (_mqttCfg.host.isEmpty()) return;
    if (_mqtt.connected()) return;
    _mqtt.setServer(_mqttCfg.host.c_str(), _mqttCfg.port);
    _mqtt.connect(HOSTNAME);
}
void mqttPublish(const String &subtopic, const String &payload) {
    mqttEnsureConnected();
    if (!_mqtt.connected()) return;
    String topic = _mqttCfg.prefix + "/" + subtopic;
    _mqtt.publish(topic.c_str(), payload.c_str(), true);
}
void mqttPublishItem(const InventoryItem &item) {
    JsonDocument doc;
    doc["name"]     = item.name;
    doc["brand"]    = item.brand;
    doc["expiry"]   = item.expiryDate;
    doc["label"]    = item.labelBarcode;
    doc["category"] = item.category;
    String out; serializeJson(doc, out);
    mqttPublish("eingelagert", out);
}
void mqttPublishRetrieve(const InventoryItem &item, int storageDays) {
    JsonDocument doc;
    doc["name"]        = item.name;
    doc["category"]    = item.category;
    doc["storageDays"] = storageDays;
    String out; serializeJson(doc, out);
    mqttPublish("ausgelagert", out);
}
void mqttCheckWarnings() {
    if (_mqttCfg.host.isEmpty()) return;
    int warn    = inventory.expiringIn(WARNING_DAYS);
    int expired = inventory.expiredCount();
    if (warn > 0 || expired > 0) {
        JsonDocument doc;
        doc["bald_ablaufend"] = warn;
        doc["abgelaufen"]     = expired;
        String out; serializeJson(doc, out);
        mqttPublish("warnung", out);
    }
}
static bool _haDiscoverySent = false;
void mqttSendHADiscovery() {
    if (_mqttCfg.host.isEmpty() || !_mqtt.connected() || _haDiscoverySent) return;
    String dev = "{\"identifiers\":[\"lager_scanner\"],\"name\":\"Lebensmittel Scanner\",\"model\":\"ESP32-S3\"}";
    auto sendDisc = [&](const char *id, const char *name, const char *stateTopic, const char *valTpl) {
        String topic = "homeassistant/sensor/lager_scanner/";
        topic += id; topic += "/config";
        JsonDocument d;
        d["name"]                    = name;
        d["state_topic"]             = String(_mqttCfg.prefix) + "/" + stateTopic;
        d["value_template"]          = valTpl;
        d["json_attributes_topic"]   = String(_mqttCfg.prefix) + "/" + stateTopic;
        d["unique_id"]               = String("lager_") + id;
        JsonDocument devDoc; deserializeJson(devDoc, dev);
        d["device"]                  = devDoc;
        String out; serializeJson(d, out);
        _mqtt.publish(topic.c_str(), out.c_str(), true);
    };
    sendDisc("eingelagert", "Eingelagert",  "eingelagert", "{{ value_json.name }}");
    sendDisc("ausgelagert", "Ausgelagert",  "ausgelagert", "{{ value_json.name }}");
    sendDisc("warnung",     "Ablaufwarnung","warnung",      "{{ value_json.bald_ablaufend }}");
    _haDiscoverySent = true;
}

// ── Telegram ─────────────────────────────────────────────────

struct TelegramCfg { String token, chatId; };
static TelegramCfg _telegramCfg;

TelegramCfg loadTelegramCfg() {
    TelegramCfg c;
    Preferences p; p.begin("telegram", true);
    c.token  = p.getString("token",  "");
    c.chatId = p.getString("chatid", "");
    p.end();
    return c;
}
void telegramSend(const String &text) {
    if (_telegramCfg.token.isEmpty() || _telegramCfg.chatId.isEmpty()) return;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient https;
    String url = "https://api.telegram.org/bot" + _telegramCfg.token + "/sendMessage";
    if (!https.begin(client, url)) return;
    https.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["chat_id"]    = _telegramCfg.chatId;
    doc["text"]       = text;
    doc["parse_mode"] = "HTML";
    String body; serializeJson(doc, body);
    https.POST(body);
    https.end();
}

// ── Buzzer-Feedback ───────────────────────────────────────────
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
inline void buzz(uint16_t freq = 2000, uint32_t dur = 80)  { tone(BUZZER_PIN, freq, dur); }
inline void buzzOk()    { buzz(2200, 80); }
inline void buzzError() { buzz(600, 300); }
#else
inline void buzz(uint16_t = 0, uint32_t = 0) {}
inline void buzzOk()    {}
inline void buzzError() {}
#endif

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
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
#endif

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
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        Serial.println("[FS] LittleFS mount failed");
        display.showBooting("FS-Fehler!"); delay(1000);
        categoryManager.begin(); // lädt Defaults auch ohne FS
    } else {
        Serial.println("[FS] LittleFS OK");
        categoryManager.begin();
        if (!inventory.begin())  Serial.println("[Inventory] Fehler!");
        customProducts.begin();
        foodCache.begin();
        storageStats.begin();
        shoppingList.begin();
    }

    { Preferences p; p.begin("dev", true); g_useNumpad = p.getBool("numpad", false); p.end(); }

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
    if (webInterface) webInterface->loop();
    serverSync.loop();
    if (!_mqttCfg.host.isEmpty()) {
        _mqtt.loop();
        if (!_mqtt.connected()) _haDiscoverySent = false;
        if (millis() - _mqttLastCheck > MQTT_CHECK_INTERVAL_MS) {
            _mqttLastCheck = millis();
            mqttEnsureConnected();
            mqttSendHADiscovery();
            mqttCheckWarnings();
            // Telegram stündliche Warnung
            int warn = inventory.expiringIn(WARNING_DAYS) + inventory.expiredCount();
            if (warn > 0)
                telegramSend("<b>⚠️ Ablauf-Warnung:</b> " + String(warn) +
                             " Artikel laufen bald ab oder sind abgelaufen!");
        }
    }
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
        logScan(bc);

        if (state == State::POWER_SAVE) {
            display.setBrightness(220);
            setState(State::MAIN);
        } else {
            bool isLabel = (bc.length() == 6 && bc[0] == 'L');
            const InventoryItem *found = isLabel ? inventory.findByLabel(bc) : nullptr;
            if (found) {
                retrieveItem = *found;
                setState(State::RETRIEVE);
            } else {
                currentBarcode = bc;
                setState(State::FETCHING);
            }
        }
        return;
    }

    // ── Power-Save-Prüfung ───────────────────────────────────
    if (state != State::POWER_SAVE &&
        state != State::BOOTING   &&
        state != State::WIFI_CONNECTING &&
        millis() - lastActivity > POWER_SAVE_MS) {
        display.setBrightness(0);
        scanner.disable();
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
            webInterface = new WebInterface(inventory, customProducts, storageStats, shoppingList);
            webInterface->begin();
            _mqttCfg      = loadMqttCfg();
            _telegramCfg  = loadTelegramCfg();
            mqttEnsureConnected();
            mqttSendHADiscovery();
            {
                auto syncCfg = ServerSync::loadConfig();
                serverSync.begin(syncCfg);
            }
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
        if (hit(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H))
            setState(State::MAIN);
        if (hit(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H) && inventory.count()>0) {
            browseIndex=0; setState(State::INVENTORY_BROWSE);
        }
        break;
    }

    // ── MAIN: Kategorie-Grid ─────────────────────────────────
    case State::MAIN: {
        if (screenDirty) {
            std::vector<int> catCounts;
            catCounts.reserve(g_categories.size());
            for (const auto &cat : g_categories)
                catCounts.push_back(inventory.countByCategory(cat.name));
            int warnCount = inventory.expiringIn(WARNING_DAYS) + inventory.expiredCount();
            display.showCategoryGrid(catCounts, warnCount, WiFi.status() == WL_CONNECTED);
            screenDirty = false;
        }
        // Tap auf Kategorie-Tile
        if (tapped && ty > CAT_HDR) {
            int col = (tx - CAT_GAP) / (CAT_TILE_W + CAT_GAP);
            int row = (ty - CAT_HDR - CAT_GAP) / (CAT_TILE_H + CAT_GAP);
            int idx = row * CAT_COLS + col;
            if (col >= 0 && col < CAT_COLS && row >= 0 && row < CAT_ROWS
                && idx < (int)g_categories.size()) {
                currentCatIndex = idx;
                mainOffset = 0;
                setState(State::PRODUCT_LIST);
            }
        }
        break;
    }

    // ── PRODUCT_LIST: Produkte einer Kategorie ───────────────
    case State::PRODUCT_LIST: {
        static std::vector<CustomProduct> listProducts;
        if (screenDirty) {
            String catName = (currentCatIndex < (int)g_categories.size())
                             ? g_categories[currentCatIndex].name : "";
            listProducts = customProducts.byCategory(catName);
            uint16_t catColor = (currentCatIndex < (int)g_categories.size())
                                ? g_categories[currentCatIndex].bgColor : COLOR_ACCENT;
            display.showProductList(catName, catColor, listProducts, mainOffset,
                                    WiFi.status() == WL_CONNECTED);
            screenDirty = false;
        }
        // Zurück
        if ((tapped && ty < SUB_HDR && tx < 120) || hardBack) {
            setState(State::MAIN); break;
        }
        // Produkt antippen
        if (tapped && ty >= SUB_HDR) {
            int idx = mainOffset + (ty - SUB_HDR) / LIST_ITEM_H;
            if (idx < (int)listProducts.size()) {
                const auto &p = listProducts[idx];
                currentProduct = { true, p.barcode, p.name, p.brand, "", "" };
                currentBarcode = p.barcode;
                if (p.defaultDays > 0) {
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
        if (gest==Gesture::SWIPE_UP || gest==Gesture::SWIPE_LEFT) {
            int maxOff = max(0,(int)listProducts.size()-LIST_MAX_VIS);
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
        ProductInfo cached = foodCache.get(currentBarcode);
        if (cached.found) {
            currentProduct = cached;
        } else if (WiFi.status()==WL_CONNECTED) {
            currentProduct = foodAPI.lookup(currentBarcode);
            if (currentProduct.found) foodCache.put(currentProduct);
        } else {
            currentProduct = { false, currentBarcode, "Unbekannt (" + currentBarcode + ")" };
        }
        initDateToday();
        setState(State::ENTER_DATE);
        break;
    }

    // ── ENTER_DATE ───────────────────────────────────────────
    case State::ENTER_DATE: {
        static bool    swipeActive = false;
        static int16_t swipeStartY = 0, swipeCurY = 0;
        static int     swipeCol    = -1;
        static int     npField     = 0;
        static String  npTyped     = "";
        bool wifiConn = (WiFi.status() == WL_CONNECTED);

        if (screenDirty) {
            swipeActive = false; npField = 0; npTyped = "";
            screenDirty = false;
            if (g_useNumpad) display.showDateEntryNumpad(dateInput, currentProduct.name, 0, "", wifiConn);
            else             display.showDateEntry(dateInput, currentProduct.name, wifiConn);
        }

        if (hardBack) { swipeActive = false; setState(State::PRODUCT_LIST); break; }

        // ── Ziffernblock-Zweig ────────────────────────────────
        if (g_useNumpad) {
            if (tapped && ty >= NP_TOP) {
                int row = (ty - NP_TOP) / NP_ROW_H;
                int col = constrain((int)(tx / NP_COL_W), 0, 2);
                if (row < 0 || row > 3) break;

                bool doAdvance = false;

                if (row == 3 && col == 0) {
                    // ← Backspace / Zurück
                    if (!npTyped.isEmpty())      npTyped.remove(npTyped.length()-1, 1);
                    else if (npField > 0)        { npField--; npTyped = ""; }
                    else                         { setState(State::PRODUCT_LIST); break; }
                } else if (row == 3 && col == 2) {
                    // → / OK: Feld bestätigen (ggf. mit 1 Stelle)
                    if (npTyped.isEmpty()) {
                        buzzOk();
                        if (npField == 2) { clampDate(dateInput); setState(State::SAVING); break; }
                        npField++; npTyped = "";
                    } else {
                        if (npTyped.length() == 1) npTyped = "0" + npTyped;
                        doAdvance = true;
                    }
                } else {
                    // Ziffer 0-9
                    int digit = (row < 3) ? (row * 3 + col + 1) : 0;
                    if (npTyped.length() < 2) {
                        npTyped += (char)('0' + digit);
                        doAdvance = (npTyped.length() == 2);
                    }
                }

                if (doAdvance) {
                    int val = npTyped.toInt();
                    int maxV = (npField == 0) ? 31 : (npField == 1) ? 12 : 99;
                    bool valid = (npField == 2) ? (val <= 99)
                                               : (val >= 1 && val <= maxV);
                    if (!valid) {
                        buzzError(); npTyped = "";
                    } else {
                        if      (npField == 0) dateInput.day   = val;
                        else if (npField == 1) dateInput.month = val;
                        else { dateInput.year = 2000 + val; clampDate(dateInput); buzzOk(); setState(State::SAVING); break; }
                        clampDate(dateInput); buzzOk(); npField++; npTyped = "";
                    }
                }

                display.showDateEntryNumpad(dateInput, currentProduct.name, npField, npTyped, wifiConn);
            }
            break;
        }

        // ── Drum-Roller-Zweig ─────────────────────────────────
        bool changed = false;

        if (tapped) {
            if (ty >= DRUM_BTN_Y) {
                if (tx < DISPLAY_W / 2) { swipeActive = false; setState(State::PRODUCT_LIST); break; }
                else                    { swipeActive = false; setState(State::SAVING);        break; }
            } else if (ty >= DRUM_ARRDWN_Y) {
                int c = constrain((int)(tx / DRUM_COL_W), 0, 2);
                if      (c == 0) dateInput.day   = constrain(dateInput.day   - 1, 1, 31);
                else if (c == 1) dateInput.month = constrain(dateInput.month - 1, 1, 12);
                else             dateInput.year  = constrain(dateInput.year  - 1, 2024, 2099);
                clampDate(dateInput); buzzOk(); changed = true;
            } else if (ty >= DRUM_TOP) {
                swipeActive = true; swipeStartY = ty; swipeCurY = ty;
                swipeCol = constrain((int)(tx / DRUM_COL_W), 0, 2);
            } else if (ty >= DRUM_ARRUP_Y) {
                int c = constrain((int)(tx / DRUM_COL_W), 0, 2);
                if      (c == 0) dateInput.day   = constrain(dateInput.day   + 1, 1, 31);
                else if (c == 1) dateInput.month = constrain(dateInput.month + 1, 1, 12);
                else             dateInput.year  = constrain(dateInput.year  + 1, 2024, 2099);
                clampDate(dateInput); buzzOk(); changed = true;
            }
        }

        if (touch.isDown() && swipeActive) {
            int16_t curY = touch.tapY();
            if (curY != swipeCurY) {
                swipeCurY = curY;
                display.showDateEntry(dateInput, currentProduct.name,
                                      wifiConn, swipeCol, swipeCurY - swipeStartY);
            }
        }

        if (!touch.isDown() && swipeActive) {
            int steps = -(swipeCurY - swipeStartY) / 20;
            if (steps != 0) {
                if      (swipeCol == 0) dateInput.day   = constrain(dateInput.day   + steps, 1, 31);
                else if (swipeCol == 1) dateInput.month = constrain(dateInput.month + steps, 1, 12);
                else if (swipeCol == 2) dateInput.year  = constrain(dateInput.year  + steps, 2024, 2099);
                clampDate(dateInput); buzzOk();
            }
            swipeActive = false; changed = true;
        }

        if (changed) display.showDateEntry(dateInput, currentProduct.name, wifiConn);
        break;
    }

    // ── SAVING ───────────────────────────────────────────────
    case State::SAVING: {
        String labelCode = generateLabelCode();
        InventoryItem item;
        item.barcode      = currentBarcode;
        item.name         = currentProduct.name;
        item.brand        = currentProduct.brand;
        item.category     = (currentCatIndex < (int)g_categories.size())
                            ? g_categories[currentCatIndex].name : "";
        item.expiryDate   = dateInputToStr(dateInput);
        item.addedDate    = todayStr();
        item.quantity     = 1;
        item.labelBarcode = labelCode;
        if (inventory.addItem(item)) {
            lastAddedItem = item;
            buzzOk();
            serverSync.onItemAdded(item);
            mqttPublishItem(item);
            telegramSend("<b>✅ Eingelagert:</b> " + item.name +
                         "\nMHD: " + isoToDisplay(item.expiryDate) +
                         "\nLabel: " + item.labelBarcode);
            setState(State::PRINTING);
        } else {
            buzzError();
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
            printer.printLabel(
                lastAddedItem.name,
                lastAddedItem.labelBarcode,
                isoToDisplay(lastAddedItem.addedDate),
                lastAddedItem.expiryDate.isEmpty() ? "" : isoToDisplay(lastAddedItem.expiryDate)
            );
        }
        if (millis()-stateEnter > 2200) {
            display.showSuccess(lastAddedItem.name, dateInputDisplay(dateInput), true);
            setState(State::SUCCESS);
        }
        break;
    }

    // ── SUCCESS ──────────────────────────────────────────────
    case State::SUCCESS: {
        int16_t halfW = (TBTN_W - 4) / 2;
        if (hit(TBTN_X, TBTN_SECONDARY_Y, halfW, TBTN_H)) {
            setState(State::PRINTING);
            break;
        }
        if (hit(TBTN_X + (TBTN_W + 4) / 2, TBTN_SECONDARY_Y, halfW, TBTN_H) ||
            hardBack || millis()-stateEnter > 8000) {
            setState(State::MAIN);
        }
        break;
    }

    // ── ERROR ────────────────────────────────────────────────
    case State::ERROR:
        if (screenDirty) { screenDirty = false; buzzError(); }
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
            int storageDays = max(0, -daysUntilExpiry(retrieveItem.addedDate));
            storageStats.record(retrieveItem, todayStr(), storageDays);
            shoppingList.add(retrieveItem.name, retrieveItem.category);
            serverSync.onItemRemoved(retrieveItem, storageDays);
            mqttPublishRetrieve(retrieveItem, storageDays);
            telegramSend("<b>\xF0\x9F\x93\xA4 Ausgelagert:</b> " + retrieveItem.name +
                         "\nLagerdauer: " + String(storageDays) + " Tage");
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
        if (screenDirty) screenDirty = false;
        if (tapped) {
            lastActivity = millis();
            display.setBrightness(220);
            scanner.enable();
            setState(State::MAIN);
        }
        break;
    }

    case State::BOOTING: break;

    } // switch
}
