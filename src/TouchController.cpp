#include "TouchController.h"
#include <Wire.h>

TouchController::TouchController(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin)
    : _sda(sda), _scl(scl), _int(intPin), _rst(rstPin) {}

bool TouchController::begin() {
    // Hardware-Reset (nur wenn RST-Pin gültig, d.h. nicht -1 → 255)
    if (_rst < 200) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(100);
    }

    pinMode(_int, INPUT);

    Wire.begin(_sda, _scl);
    Wire.setClock(400000);

    // Verbindungstest
    Wire.beginTransmission(I2C_ADDR);
    _initialized = (Wire.endTransmission() == 0);
    if (!_initialized) Serial.printf("[Touch] FT3168 nicht gefunden (I2C 0x%02X)\n", I2C_ADDR);
    else               Serial.println("[Touch] FT3168 OK");
    return _initialized;
}

void TouchController::update() {
    if (!_initialized) return;
    _tapped  = false;
    _gesture = Gesture::NONE;

    bool pinLow = (digitalRead(_int) == LOW);

    if (pinLow && !_wasDown) {
        _wasDown = true;
        readRegisters();
    } else if (!pinLow) {
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

void TouchController::readRegisters() {
    // FT3168 Register-Layout ab 0x01:
    // [0] GestureID  [1] TD_STATUS (touch count)
    // [2] P1_XH      [3] P1_XL
    // [4] P1_YH      [5] P1_YL
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0x01);
    if (Wire.endTransmission(false) != 0) return;

    Wire.requestFrom(I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6) return;

    uint8_t gest = Wire.read();
    uint8_t num  = Wire.read();
    uint8_t xH   = Wire.read();
    uint8_t xL   = Wire.read();
    uint8_t yH   = Wire.read();
    uint8_t yL   = Wire.read();

    if ((num & 0x0F) == 0) return;

    // FT3168 Gesture-Codes → unsere Gesture-Enum
    switch (gest) {
        case 0x10: _gesture = Gesture::SWIPE_UP;    break;
        case 0x18: _gesture = Gesture::SWIPE_DOWN;  break;
        case 0x1C: _gesture = Gesture::SWIPE_LEFT;  break;
        case 0x14: _gesture = Gesture::SWIPE_RIGHT; break;
        default:   _gesture = Gesture::SINGLE_CLICK; break;
    }

    // Raw-Koordinaten im Portrait-Raum (280×456)
    int16_t raw_x = ((xH & 0x0F) << 8) | xL;
    int16_t raw_y = ((yH & 0x0F) << 8) | yL;

    // Umrechnung für setRotation(1): Portrait → Landscape 456×280
    _x = raw_y;
    _y = 279 - raw_x;
    _tapped  = true;

    Serial.printf("[Touch] raw=%d,%d  disp=%d,%d\n", raw_x, raw_y, _x, _y);
}
