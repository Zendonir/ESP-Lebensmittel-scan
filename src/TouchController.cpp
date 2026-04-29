#include "TouchController.h"
#include <Wire.h>

TouchController::TouchController(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin)
    : _sda(sda), _scl(scl), _int(intPin), _rst(rstPin) {}

bool TouchController::begin() {
    // Hardware-Reset
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(100);

    pinMode(_int, INPUT);

    Wire.begin(_sda, _scl);
    Wire.setClock(400000);

    // Verbindungstest
    Wire.beginTransmission(I2C_ADDR);
    _initialized = (Wire.endTransmission() == 0);
    if (!_initialized) Serial.printf("[Touch] CST816S nicht gefunden (I2C 0x%02X)\n", I2C_ADDR);
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
    // CST816S Register-Layout ab 0x01:
    // [0] GestureID  [1] FingerNum
    // [2] XposH      [3] XposL
    // [4] YposH      [5] YposL
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

    if (num == 0) return;

    _gesture = static_cast<Gesture>(gest);
    _x       = ((xH & 0x0F) << 8) | xL;
    _y       = ((yH & 0x0F) << 8) | yL;
    _tapped  = true;

    Serial.printf("[Touch] gest=0x%02X x=%d y=%d\n", gest, _x, _y);
}
