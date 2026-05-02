#include "BarcodeScanner.h"
#include <Preferences.h>

BarcodeScanner::BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud) {}

// ── Initialisierung ───────────────────────────────────────────

void BarcodeScanner::begin() {
    _btMode = false;
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _stream = &_serial;
    _buffer.reserve(32);
}

void BarcodeScanner::beginBT(const String &deviceName) {
    _btMode = true;
    _bt.begin(deviceName);   // ESP32 als SPP-Server
    _stream = &_bt;
    _buffer.reserve(32);
    Serial.printf("[BT] SPP-Server gestartet als \"%s\"\n", deviceName.c_str());
}

// ── Barcode lesen ─────────────────────────────────────────────

bool BarcodeScanner::available() {
    if (!_stream) return _ready;
    while (_stream->available()) {
        char c = (char)_stream->read();
        if (c == '\r' || c == '\n') {
            if (_buffer.length() > 3) {
                _ready = true;
                return true;
            }
            _buffer = "";
        } else if (isPrintable(c)) {
            _buffer += c;
        }
    }
    return _ready;
}

String BarcodeScanner::getBarcode() {
    String code = _buffer;
    code.trim();
    _buffer = "";
    _ready  = false;
    return code;
}

void BarcodeScanner::flush() {
    _buffer = "";
    _ready  = false;
    if (_stream) while (_stream->available()) _stream->read();
}

// ── Ein-/Ausschalten ──────────────────────────────────────────

void BarcodeScanner::enable() {
    if (_btMode) return;  // BT-Scanner steuert sich selbst
    // GM861: Automatik-Modus aktivieren
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x01,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

void BarcodeScanner::disable() {
    if (_btMode) return;  // BT-Scanner steuert sich selbst
    // GM861: Schlafen
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x00,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

// ── Konfiguration (NVS) ───────────────────────────────────────

bool BarcodeScanner::loadBTMode() {
    Preferences p; p.begin("scanner", true);
    bool bt = p.getBool("bt", false);
    p.end();
    return bt;
}

String BarcodeScanner::loadBTName() {
    Preferences p; p.begin("scanner", true);
    String name = p.getString("btname", "Lager-Scanner");
    p.end();
    return name;
}

void BarcodeScanner::saveScannerConfig(bool btMode, const String &btName) {
    Preferences p; p.begin("scanner", false);
    p.putBool("bt",       btMode);
    p.putString("btname", btName);
    p.end();
}
