#include "Beeper.h"
#include "config.h"

Beeper& Beeper::instance() {
    static Beeper inst;
    return inst;
}

bool Beeper::begin() {
#if !defined(I2S_SPK_BCK) || I2S_SPK_BCK < 0
    Serial.println("[Beeper] I2S_SPK_BCK nicht definiert – kein Lautsprecher");
    return false;
#else
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    if (i2s_new_channel(&chan_cfg, &_tx, nullptr) != ESP_OK) {
        Serial.println("[Beeper] i2s_new_channel FEHLER");
        return false;
    }

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100);
    std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = (gpio_num_t)I2S_SPK_BCK;
    std_cfg.gpio_cfg.ws   = (gpio_num_t)I2S_SPK_WS;
    std_cfg.gpio_cfg.dout = (gpio_num_t)I2S_SPK_DOUT;
    std_cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;

    if (i2s_channel_init_std_mode(_tx, &std_cfg) != ESP_OK) {
        Serial.println("[Beeper] I2S init FEHLER");
        i2s_del_channel(_tx); _tx = nullptr; return false;
    }
    if (i2s_channel_enable(_tx) != ESP_OK) {
        Serial.println("[Beeper] I2S enable FEHLER");
        i2s_del_channel(_tx); _tx = nullptr; return false;
    }

    _ready = true;
    Serial.printf("[Beeper] I2S OK  BCK=%d WS=%d DOUT=%d\n",
                  I2S_SPK_BCK, I2S_SPK_WS, I2S_SPK_DOUT);
    return true;
#endif
}

void Beeper::tone(uint16_t freq, uint32_t durationMs) {
    if (!_ready || freq == 0 || durationMs == 0) return;

    static constexpr uint32_t RATE = 44100;
    // Halbe Periode in Samples für Rechteck-Welle
    uint32_t halfPeriod = RATE / (freq * 2);
    if (halfPeriod < 1) halfPeriod = 1;
    uint32_t totalSamples = RATE * durationMs / 1000;

    static const int CHUNK = 128;
    int16_t buf[CHUNK * 2];  // stereo L/R
    size_t written = 0;
    uint32_t s = 0;
    uint32_t hpCount = 0;
    int16_t level = 5000;

    while (s < totalSamples) {
        int n = min((int)(totalSamples - s), CHUNK);
        for (int i = 0; i < n; i++, s++) {
            if (++hpCount >= halfPeriod) { level = -level; hpCount = 0; }
            buf[i*2]   = level;
            buf[i*2+1] = level;
        }
        i2s_channel_write(_tx, buf, n * 4, &written, pdMS_TO_TICKS(100));
    }
    // kurze Stille, damit Nachklang abbricht
    memset(buf, 0, CHUNK * 4);
    i2s_channel_write(_tx, buf, CHUNK * 4, &written, pdMS_TO_TICKS(20));
}
