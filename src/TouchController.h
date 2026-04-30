#pragma once
#include <Arduino.h>
#include "driver/i2c_master.h"

// FT3168 kapazitiver Touch-Controller (I2C, ESP-IDF Master API)
enum class Gesture : uint8_t {
    NONE         = 0x00,
    SWIPE_DOWN   = 0x01,
    SWIPE_UP     = 0x02,
    SWIPE_LEFT   = 0x03,
    SWIPE_RIGHT  = 0x04,
    SINGLE_CLICK = 0x05,
    DOUBLE_CLICK = 0x0B,
    LONG_PRESS   = 0x0C
};

class TouchController {
public:
    TouchController(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin);

    bool begin();
    void update();

    bool    wasTapped();
    Gesture lastGesture() const { return _gesture; }
    int16_t tapX() const { return _x; }
    int16_t tapY() const { return _y; }
    bool    hits(int16_t bx, int16_t by, int16_t bw, int16_t bh) const;

private:
    uint8_t _sda, _scl, _int, _rst;
    bool    _initialized = false;
    bool    _wasDown     = false;
    bool    _tapped      = false;
    unsigned long _lastPoll = 0;
    int16_t _x = 0, _y = 0;
    Gesture _gesture = Gesture::NONE;

    static constexpr uint8_t  I2C_ADDR = 0x38;

    i2c_master_bus_handle_t _i2c_bus = nullptr;
    i2c_master_dev_handle_t _i2c_dev = nullptr;

    bool readRegisters();
};
