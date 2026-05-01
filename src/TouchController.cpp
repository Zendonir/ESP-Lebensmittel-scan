#include "TouchController.h"

TouchController::TouchController(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin)
    : _sda(sda), _scl(scl), _int(intPin), _rst(rstPin) {}

bool TouchController::begin() {
    // I2C Master Bus (ESP-IDF API, kein Wire)
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port                    = I2C_NUM_0;
    bus_cfg.sda_io_num                  = (gpio_num_t)_sda;
    bus_cfg.scl_io_num                  = (gpio_num_t)_scl;
    bus_cfg.clk_source                  = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt           = 7;
    bus_cfg.flags.enable_internal_pullup = 1;

    if (i2c_new_master_bus(&bus_cfg, &_i2c_bus) != ESP_OK) {
        Serial.println("[Touch] I2C Bus init FEHLER");
        return false;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = I2C_ADDR;
    dev_cfg.scl_speed_hz    = 300000;

    if (i2c_master_bus_add_device(_i2c_bus, &dev_cfg, &_i2c_dev) != ESP_OK) {
        Serial.println("[Touch] FT3168 add device FEHLER");
        return false;
    }

    // Verbindungstest: Register 0x02 lesen
    uint8_t reg = 0x02, val = 0;
    esp_err_t err = i2c_master_transmit_receive(_i2c_dev, &reg, 1, &val, 1, 10);
    _initialized = (err == ESP_OK);

    if (!_initialized) Serial.printf("[Touch] FT3168 NICHT gefunden (err=%d)\n", err);
    else               Serial.println("[Touch] FT3168 OK");
    return _initialized;
}

void TouchController::update() {
    if (!_initialized) return;
    if (millis() - _lastPoll < 20) return;
    _lastPoll = millis();

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
    // FT3168 ab Register 0x02: [TD_STATUS, XH, XL, YH, YL]
    uint8_t reg = 0x02;
    uint8_t buf[5] = {};
    esp_err_t err = i2c_master_transmit_receive(_i2c_dev, &reg, 1, buf, 5, 10);
    if (err != ESP_OK) return false;

    uint8_t touches = buf[0] & 0x0F;
    if (touches == 0) return false;

    int16_t raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
    int16_t raw_y = ((buf[3] & 0x0F) << 8) | buf[4];

    // Portrait (280×456) → Landscape (456×280)
    _x = raw_y;
    _y = 279 - raw_x;

    return true;
}
