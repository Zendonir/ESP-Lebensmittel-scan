#include "BarcodeScanner.h"
#include <Preferences.h>
#include <NimBLEDevice.h>

// ── Statischer Scanner-Zeiger für NimBLE-Callbacks ────────────

static BarcodeScanner *s_scanner    = nullptr;
static NimBLEClient   *s_client     = nullptr;
static bool            s_nimbleInit = false;

// ── Geräte-Suche (Discovery) ─────────────────────────────────

static std::vector<BLEDeviceInfo> s_discovered;
static volatile bool              s_discovering = false;
static SemaphoreHandle_t          s_discMutex   = nullptr;

// ── HID-Keycode → ASCII ───────────────────────────────────────

static char hidKeycode(uint8_t mod, uint8_t key) {
    bool shift = mod & 0x22;
    if (key >= 0x04 && key <= 0x1D) {
        char c = 'a' + (key - 0x04);
        return shift ? (char)(c - 32) : c;
    }
    if (key >= 0x1E && key <= 0x27) {
        const char *nums       = "1234567890";
        const char *shift_nums = "!@#$%^&*()";
        return shift ? shift_nums[key - 0x1E] : nums[key - 0x1E];
    }
    switch (key) {
        case 0x28: return '\n';
        case 0x2C: return ' ';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        default:   return 0;
    }
}

// ── NimBLE-Notify-Callback ────────────────────────────────────

static void hidNotifyCB(NimBLERemoteCharacteristic *, uint8_t *data,
                        size_t len, bool) {
    if (s_scanner && len >= 8) s_scanner->_onHIDReport(data, len);
}

// ── BLE-Client-Callbacks (Disconnect → Scan neu starten) ──────

class BLEClientCB : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient *, int reason) override {
        Serial.printf("[BLE] Verbindung getrennt (reason=%d)\n", reason);
        if (s_scanner) {
            s_scanner->_setConnected(false);
            // Während Discovery den HID-Scan NICHT neu starten –
            // onScanEnd macht das nach Abschluss der Suche.
            if (!s_discovering) s_scanner->_startScan();
        }
    }
};

static BLEClientCB s_clientCB;

// ── BLE-Scan-Callbacks (Gerät gefunden → verbinden) ───────────
// Muss VOR BLEDiscoveryCB definiert sein (onScanEnd referenziert s_scanCB)

class BLEScanCB : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override {
        if (!s_scanner) return;

        bool nameMatch = false;
        if (!s_scanner->_targetName.isEmpty()) {
            String found = dev->getName().c_str();
            if (found.indexOf(s_scanner->_targetName) < 0) return;
            nameMatch = true;
        }
        if (!nameMatch && !dev->isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) return;

        // Gerät sofort in die Ergebnisliste schreiben (für Web-UI)
        if (!s_discMutex) s_discMutex = xSemaphoreCreateMutex();
        BLEDeviceInfo info;
        info.name = dev->getName().c_str();
        info.addr = dev->getAddress().toString().c_str();
        info.rssi = dev->getRSSI();
        if (info.name.isEmpty()) info.name = "(kein Name)";
        xSemaphoreTake(s_discMutex, portMAX_DELAY);
        bool dup = false;
        for (auto &d : s_discovered) if (d.addr == info.addr) { dup = true; break; }
        if (!dup) s_discovered.push_back(info);
        xSemaphoreGive(s_discMutex);

        Serial.printf("[BLE] HID-Scanner gefunden: %s\n", dev->toString().c_str());
        NimBLEDevice::getScan()->stop();

        if (!s_client) {
            s_client = NimBLEDevice::createClient();
            s_client->setClientCallbacks(&s_clientCB, false);
        }

        if (!s_client->connect(dev)) {
            Serial.println("[BLE] Verbindung fehlgeschlagen, Neuversuch...");
            s_scanner->_startScan();
            return;
        }
        Serial.println("[BLE] Verbunden!");

        NimBLERemoteService *svc = s_client->getService(NimBLEUUID((uint16_t)0x1812));
        if (!svc) {
            Serial.println("[BLE] HID-Service nicht gefunden");
            s_client->disconnect();
            s_scanner->_startScan();
            return;
        }

        bool ok = false;
        const auto &chars = svc->getCharacteristics(true);
        for (auto *chr : chars) {
            if (chr->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && chr->canNotify()) {
                if (chr->subscribe(true, hidNotifyCB)) {
                    Serial.println("[BLE] HID-Report abonniert");
                    ok = true;
                }
            }
        }

        if (ok) {
            s_scanner->_setConnected(true);
        } else {
            Serial.println("[BLE] Kein notify-fähiges Report-Char");
            s_client->disconnect();
            s_scanner->_startScan();
        }
    }
};

static BLEScanCB s_scanCB;

// ── Discovery-Callbacks (alle Geräte, kein Filter) ───────────

class BLEDiscoveryCB : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override {
        BLEDeviceInfo info;
        info.name = dev->getName().c_str();
        info.addr = dev->getAddress().toString().c_str();
        info.rssi = dev->getRSSI();
        if (info.name.isEmpty()) info.name = "(kein Name)";
        Serial.printf("[BLE] Disc: %-24s %s %ddBm\n",
                      info.name.c_str(), info.addr.c_str(), info.rssi);
        xSemaphoreTake(s_discMutex, portMAX_DELAY);
        bool dup = false;
        for (auto &d : s_discovered) if (d.addr == info.addr) { dup = true; break; }
        if (!dup) s_discovered.push_back(info);
        xSemaphoreGive(s_discMutex);
    }

    void onScanEnd(const NimBLEScanResults &, int reason) override {
        xSemaphoreTake(s_discMutex, portMAX_DELAY);
        int cnt = s_discovered.size();
        xSemaphoreGive(s_discMutex);
        Serial.printf("[BLE] onScanEnd: %d Geräte, reason=%d\n", cnt, reason);
        s_discovering = false;
        // HID-Scan nach Abschluss wieder aufnehmen
        if (s_scanner && s_scanner->isBLEMode()) {
            NimBLEDevice::getScan()->setScanCallbacks(&s_scanCB, false);
            s_scanner->_startScan();
        }
    }
};

static BLEDiscoveryCB s_discoveryCB;

// ── Geräte-Suche Implementierung ─────────────────────────────
//
// Problem: scan->start() schlägt fehl wenn NimBLE intern noch eine
// connect()-Prozedur (aus BLEScanCB::onResult) abarbeitet.
// Lösung: FreeRTOS-Task mit Retry-Loop bis NimBLE bereit ist.

void BarcodeScanner::startDiscoveryScan(int durationSec) {
    if (s_discovering) return;
    if (!s_discMutex) s_discMutex = xSemaphoreCreateMutex();
    // Gefundene HID-Geräte NICHT löschen – der HID-Scan hat evtl. schon welche gesammelt

    if (!s_nimbleInit) {
        NimBLEDevice::init("");
        s_nimbleInit = true;
    }

    NimBLEScan *scan = NimBLEDevice::getScan();
    Serial.printf("[BLE] Discovery: isScanning=%d connected=%d seen=%d\n",
                  (int)scan->isScanning(),
                  s_scanner ? (int)s_scanner->_bleConnected : -1,
                  (int)s_discovered.size());

    // Wenn HID-Scan bereits Geräte geliefert hat → sofort fertig
    xSemaphoreTake(s_discMutex, portMAX_DELAY);
    int already = s_discovered.size();
    xSemaphoreGive(s_discMutex);
    if (already > 0) {
        Serial.printf("[BLE] Discovery: %d Gerät(e) bereits bekannt, kein Scan nötig\n", already);
        return;
    }

    // Noch keine Geräte bekannt → Discovery-Scan starten (mit Retry)
    s_discovering = true;
    int *durPtr = new int(durationSec);
    xTaskCreate([](void *arg) {
        int dur = *reinterpret_cast<int *>(arg);
        delete reinterpret_cast<int *>(arg);

        NimBLEScan *scan = NimBLEDevice::getScan();
        if (scan->isScanning()) { scan->stop(); delay(300); }

        scan->setScanCallbacks(&s_discoveryCB, false);
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(99);

        bool ok = false;
        for (int i = 0; i < 20 && !ok; i++) {
            ok = scan->start((uint32_t)dur * 1000, false, true);
            if (!ok) {
                Serial.printf("[BLE] Discovery retry %d/20\n", i + 1);
                delay(500);
            }
        }
        if (!ok) {
            Serial.println("[BLE] Discovery: start() fehlgeschlagen");
            s_discovering = false;
        }
        vTaskDelete(nullptr);
    }, "bleDisc", 4096, durPtr, 1, nullptr);
}

bool BarcodeScanner::isDiscovering() { return s_discovering; }

std::vector<BLEDeviceInfo> BarcodeScanner::getDiscoveredDevices() {
    if (!s_discMutex) return {};
    xSemaphoreTake(s_discMutex, portMAX_DELAY);
    auto copy = s_discovered;
    xSemaphoreGive(s_discMutex);
    return copy;
}

// ── Konstruktor ───────────────────────────────────────────────

BarcodeScanner::BarcodeScanner(HardwareSerial &serial, uint8_t rxPin,
                               uint8_t txPin, uint32_t baud)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

// ── begin() – UART-Modus ──────────────────────────────────────

void BarcodeScanner::begin() {
    _bleMode = false;
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _uartBuf.reserve(32);
}

// ── beginBLE() – BLE HID-Client-Modus ────────────────────────

void BarcodeScanner::beginBLE(const String &targetName) {
    _bleMode    = true;
    _targetName = targetName;
    _mutex      = xSemaphoreCreateMutex();
    s_scanner   = this;
    _bleBuf.reserve(32);

    if (!s_nimbleInit) {
        NimBLEDevice::init("");
        s_nimbleInit = true;
    }
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&s_scanCB, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    Serial.printf("[BLE] Suche HID-Scanner%s\n",
                  targetName.isEmpty() ? "" : (", Filter: \"" + targetName + "\"").c_str());
    _startScan();
}

void BarcodeScanner::_startScan() {
    // Während Discovery den HID-Scan nicht starten
    if (s_discovering) return;
    if (!NimBLEDevice::getScan()->isScanning())
        NimBLEDevice::getScan()->start(0, false);
}

// ── HID-Report verarbeiten ─────────────────────────────────────

void BarcodeScanner::_onHIDReport(const uint8_t *data, size_t len) {
    if (!_mutex) return;
    uint8_t mod = data[0];
    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (size_t i = 2; i < len && i < 8; i++) {
        if (data[i] == 0) continue;
        char c = hidKeycode(mod, data[i]);
        if (c == '\n') {
            if (_bleBuf.length() > 3) _bleReady = true;
        } else if (c) {
            _bleBuf += c;
        }
    }
    xSemaphoreGive(_mutex);
}

// ── available() / getBarcode() / flush() ──────────────────────

bool BarcodeScanner::available() {
    if (_bleMode) return _bleReady;
    while (_serial.available()) {
        char c = (char)_serial.read();
        if (c == '\r' || c == '\n') {
            if (_uartBuf.length() > 3) { _uartReady = true; return true; }
            _uartBuf = "";
        } else if (isPrintable(c)) {
            _uartBuf += c;
        }
    }
    return _uartReady;
}

String BarcodeScanner::getBarcode() {
    if (_bleMode) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        String code = _bleBuf;
        _bleBuf   = "";
        _bleReady = false;
        xSemaphoreGive(_mutex);
        code.trim();
        return code;
    }
    String code = _uartBuf;
    code.trim();
    _uartBuf   = "";
    _uartReady = false;
    return code;
}

void BarcodeScanner::flush() {
    if (_bleMode) {
        if (_mutex) {
            xSemaphoreTake(_mutex, portMAX_DELAY);
            _bleBuf = ""; _bleReady = false;
            xSemaphoreGive(_mutex);
        }
        return;
    }
    _uartBuf = ""; _uartReady = false;
    while (_serial.available()) _serial.read();
}

// ── enable() / disable() ──────────────────────────────────────

void BarcodeScanner::enable() {
    if (_bleMode) return;
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x01,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

void BarcodeScanner::disable() {
    if (_bleMode) return;
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x00,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

// ── NVS-Konfiguration ─────────────────────────────────────────

bool BarcodeScanner::loadBTMode() {
    Preferences p; p.begin("scanner", true);
    bool bt = p.getBool("bt", false); p.end(); return bt;
}

String BarcodeScanner::loadBTName() {
    Preferences p; p.begin("scanner", true);
    String n = p.getString("btname", ""); p.end(); return n;
}

void BarcodeScanner::saveScannerConfig(bool btMode, const String &btName) {
    Preferences p; p.begin("scanner", false);
    p.putBool("bt",       btMode);
    p.putString("btname", btName);
    p.end();
}
