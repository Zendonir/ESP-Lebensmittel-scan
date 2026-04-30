#include "TouchController.h"
#include <Wire.h>

TouchController::TouchController(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin)
    : _sda(sda), _scl(scl), _int(intPin), _rst(rstPin) {}

bool TouchController::begin() {
    if (_rst < 200) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW); delay(10);
        digitalWrite(_rst, HIGH); delay(100);
    }

    Wire.begin(_sda, _scl);
    Wire.setClock(300000);

    // I2C-Scan (Debug)
    Serial.printf("[Touch] I2C scan SDA=%d SCL=%d ...\n", _sda, _scl);
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
            Serial.printf("[Touch] Gefunden: 0x%02X\n", addr);
    }

    Wire.beginTransmission(I2C_ADDR);
    _initialized = (Wire.endTransmission() == 0);
    if (!_initialized) Serial.printf("[Touch] FT3168 NICHT gefunden (0x%02X)\n", I2C_ADDR);
    else               Serial.println("[Touch] FT3168 OK");
    return _initialized;
}

void TouchController::update() {
    if (!_initialized) return;
    _tapped  = false;
    _gesture = Gesture::NONE;

    bool touching = readRegisters();
    if (touching && !_wasDown) {
        _wasDown = true;
        _tapped  = true;
    } else if (!touching) {
        _wasDown = false;
    }
}

bool TouchController::wasTapped() {
    bool v = _tapped;
    _tapped = false;
    return v;
}

bool TouchController::hits(int16_t bx, int16_t by, int16_t bw, int16_t bh) const {
    return _x >= bx && _x < bx + bw && _y >= by && _y < by + bh;
}

bool TouchController::readRegisters() {
    // FT3168: ab Register 0x02 → 5 Bytes lesen (wie Referenz-Projekt)
    // [0] TD_STATUS  [1] P1_XH  [2] P1_XL  [3] P1_YH  [4] P1_YL
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom(I2C_ADDR, (uint8_t)5);
    if (Wire.available() < 5) return false;

    uint8_t num = Wire.read();
    uint8_t xH  = Wire.read();
    uint8_t xL  = Wire.read();
    uint8_t yH  = Wire.read();
    uint8_t yL  = Wire.read();

    if ((num & 0x0F) == 0) return false;

    int16_t raw_x = ((xH & 0x0F) << 8) | xL;
    int16_t raw_y = ((yH & 0x0F) << 8) | yL;

    // Portrait (280×456) → Landscape (456×280) nach setRotation(1)
    _x = raw_y;
    _y = 279 - raw_x;

    Serial.printf("[Touch] raw=%d,%d  disp=%d,%d\n", raw_x, raw_y, _x, _y);
    return true;
}
