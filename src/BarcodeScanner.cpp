#include "BarcodeScanner.h"

BarcodeScanner::BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud)
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud), _ready(false) {}

void BarcodeScanner::begin() {
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _buffer.reserve(32);
    Serial.printf("[Scanner] begin() RX=GPIO%d TX=GPIO%d baud=%d\n", _rxPin, _txPin, _baud);
}

void BarcodeScanner::debugDump() {
    static unsigned long _lastDbg = 0;
    if (millis() - _lastDbg < 2000) return;
    _lastDbg = millis();
    int n = _serial.available();
    if (n > 0) {
        Serial.printf("[Scanner] %d Bytes im Buffer: ", n);
        while (_serial.available()) {
            uint8_t b = _serial.read();
            Serial.printf("0x%02X ", b);
            if (isPrintable(b)) _buffer += (char)b;
            else if (b == '\r' || b == '\n') {
                if (_buffer.length() > 3) _ready = true;
                else _buffer = "";
            }
        }
        Serial.println();
    } else {
        Serial.println("[Scanner] keine Daten auf RX");
    }
}

bool BarcodeScanner::available() {
    while (_serial.available()) {
        char c = _serial.read();
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
    _ready = false;
    return code;
}

void BarcodeScanner::enable() {
    // GM861: Scanner aktivieren (Automatik-Modus)
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x01,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

void BarcodeScanner::disable() {
    // GM861: Scanner deaktivieren (schläft, kein Scan)
    static const uint8_t cmd[] = {0x7E,0x00,0x08,0x01,0x00,0x02,0x00,0xAB,0xCD};
    _serial.write(cmd, sizeof(cmd));
}

void BarcodeScanner::flush() {
    _buffer = "";
    _ready = false;
    while (_serial.available()) _serial.read();
}
