#pragma once
#include <Arduino.h>
#include "driver/i2s_std.h"

// Tonausgabe über I2S-Verstärker (onboard-Lautsprecher).
// GPIO-Pins in config.h (I2S_SPK_BCK / I2S_SPK_WS / I2S_SPK_DOUT)
// müssen aus dem Waveshare-Schaltplan des ESP32-S3-Touch-LCD-3.5 entnommen werden.
class Beeper {
public:
    static Beeper& instance();

    bool begin();
    void tone(uint16_t freq, uint32_t durationMs);
    bool ok() const { return _ready; }

private:
    Beeper() {}
    bool _ready = false;
    i2s_chan_handle_t _tx = nullptr;
};
