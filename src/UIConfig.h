#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct UIConfig {
    uint16_t bg;
    uint16_t text;
    uint16_t subtext;
    uint16_t ok;
    uint16_t warn;
    uint16_t danger;
    uint16_t accent;
    uint16_t header;
    uint16_t surface;
    uint16_t btn_ok;
    uint16_t btn_back;
    uint8_t  brightness;
    uint8_t  warning_days;
    uint8_t  danger_days;
};

extern UIConfig g_uiCfg;

inline void resetUIConfig(UIConfig &c) {
    c.bg          = 0x0000;
    c.text        = 0xFFFF;
    c.subtext     = 0x8410;
    c.ok          = 0x07E0;
    c.warn        = 0xFD20;
    c.danger      = 0xF800;
    c.accent      = 0x07FF;
    c.header      = 0x1926;
    c.surface     = 0x18C3;
    c.btn_ok      = 0x0640;
    c.btn_back    = 0x2945;
    c.brightness  = 220;
    c.warning_days = 7;
    c.danger_days  = 3;
}

inline void loadUIConfig() {
    resetUIConfig(g_uiCfg);
    Preferences p; p.begin("uicfg", true);
    g_uiCfg.bg          = p.getUShort("bg",      g_uiCfg.bg);
    g_uiCfg.text        = p.getUShort("text",     g_uiCfg.text);
    g_uiCfg.subtext     = p.getUShort("subtext",  g_uiCfg.subtext);
    g_uiCfg.ok          = p.getUShort("ok",       g_uiCfg.ok);
    g_uiCfg.warn        = p.getUShort("warn",     g_uiCfg.warn);
    g_uiCfg.danger      = p.getUShort("danger",   g_uiCfg.danger);
    g_uiCfg.accent      = p.getUShort("accent",   g_uiCfg.accent);
    g_uiCfg.header      = p.getUShort("header",   g_uiCfg.header);
    g_uiCfg.surface     = p.getUShort("surface",  g_uiCfg.surface);
    g_uiCfg.btn_ok      = p.getUShort("btn_ok",   g_uiCfg.btn_ok);
    g_uiCfg.btn_back    = p.getUShort("btn_back", g_uiCfg.btn_back);
    g_uiCfg.brightness  = p.getUChar("bright",    g_uiCfg.brightness);
    g_uiCfg.warning_days= p.getUChar("warn_d",    g_uiCfg.warning_days);
    g_uiCfg.danger_days = p.getUChar("danger_d",  g_uiCfg.danger_days);
    p.end();
}

inline void saveUIConfig() {
    Preferences p; p.begin("uicfg", false);
    p.putUShort("bg",      g_uiCfg.bg);
    p.putUShort("text",    g_uiCfg.text);
    p.putUShort("subtext", g_uiCfg.subtext);
    p.putUShort("ok",      g_uiCfg.ok);
    p.putUShort("warn",    g_uiCfg.warn);
    p.putUShort("danger",  g_uiCfg.danger);
    p.putUShort("accent",  g_uiCfg.accent);
    p.putUShort("header",  g_uiCfg.header);
    p.putUShort("surface", g_uiCfg.surface);
    p.putUShort("btn_ok",  g_uiCfg.btn_ok);
    p.putUShort("btn_back",g_uiCfg.btn_back);
    p.putUChar("bright",   g_uiCfg.brightness);
    p.putUChar("warn_d",   g_uiCfg.warning_days);
    p.putUChar("danger_d", g_uiCfg.danger_days);
    p.end();
}

// "#RRGGBB" → RGB565
inline uint16_t hexToRGB565(const String &hex) {
    if (hex.length() < 7) return 0;
    long v = strtol(hex.c_str() + 1, nullptr, 16);
    uint8_t r = (v >> 16) & 0xFF;
    uint8_t g = (v >> 8)  & 0xFF;
    uint8_t b =  v        & 0xFF;
    return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
}

// RGB565 → "#RRGGBB"
inline String rgb565ToHex(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5)  & 0x3F) << 2;
    uint8_t b = ( c        & 0x1F) << 3;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return String(buf);
}
