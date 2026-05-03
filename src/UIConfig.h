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
    uint8_t  tbtn_h;      // button height (default 48)
    uint8_t  cat_cols;    // category columns 1-3 (default 2)
    uint8_t  cat_gap;     // tile gap px (default 6)
    uint8_t  list_item_h; // list item height (default 64)
    uint8_t  btn_radius;    // button corner radius (default 10)
    uint8_t  tbtn_margin;   // button side margin px (default 8)
    uint8_t  list_radius;   // list item corner radius (default 6)
    uint8_t  sub_hdr_h;     // sub-header height px (default 54)
    uint8_t  cat_hdr_h;     // category header height px (default 44)
    uint8_t  power_save_min; // power save timeout minutes, 0=never (default 10)
    uint8_t  date_format;   // 0=DD.MM.YYYY  1=MM/DD/YYYY  2=YYYY-MM-DD (default 0)
    uint8_t  show_clock;    // show clock on screens (default 1)
    uint8_t  sound_ok;      // buzzer on success (default 1)
    uint8_t  sound_err;     // buzzer on error (default 1)
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
    c.tbtn_h        = 48;
    c.cat_cols      = 2;
    c.cat_gap       = 6;
    c.list_item_h   = 64;
    c.btn_radius    = 10;
    c.tbtn_margin   = 8;
    c.list_radius   = 6;
    c.sub_hdr_h     = 54;
    c.cat_hdr_h     = 44;
    c.power_save_min = 10;
    c.date_format   = 0;
    c.show_clock    = 1;
    c.sound_ok      = 1;
    c.sound_err     = 1;
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
    g_uiCfg.tbtn_h        = p.getUChar("tbtn_h",      g_uiCfg.tbtn_h);
    g_uiCfg.cat_cols      = p.getUChar("cat_cols",    g_uiCfg.cat_cols);
    g_uiCfg.cat_gap       = p.getUChar("cat_gap",     g_uiCfg.cat_gap);
    g_uiCfg.list_item_h   = p.getUChar("list_item_h", g_uiCfg.list_item_h);
    g_uiCfg.btn_radius    = p.getUChar("btn_radius",  g_uiCfg.btn_radius);
    g_uiCfg.tbtn_margin   = p.getUChar("tbtn_marg",   g_uiCfg.tbtn_margin);
    g_uiCfg.list_radius   = p.getUChar("list_rad",    g_uiCfg.list_radius);
    g_uiCfg.sub_hdr_h     = p.getUChar("sub_hdr_h",   g_uiCfg.sub_hdr_h);
    g_uiCfg.cat_hdr_h     = p.getUChar("cat_hdr_h",   g_uiCfg.cat_hdr_h);
    g_uiCfg.power_save_min= p.getUChar("pwr_save_m",  g_uiCfg.power_save_min);
    g_uiCfg.date_format   = p.getUChar("date_fmt",    g_uiCfg.date_format);
    g_uiCfg.show_clock    = p.getUChar("show_clock",  g_uiCfg.show_clock);
    g_uiCfg.sound_ok      = p.getUChar("sound_ok",    g_uiCfg.sound_ok);
    g_uiCfg.sound_err     = p.getUChar("sound_err",   g_uiCfg.sound_err);
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
    p.putUChar("bright",    g_uiCfg.brightness);
    p.putUChar("warn_d",    g_uiCfg.warning_days);
    p.putUChar("danger_d",  g_uiCfg.danger_days);
    p.putUChar("tbtn_h",     g_uiCfg.tbtn_h);
    p.putUChar("cat_cols",   g_uiCfg.cat_cols);
    p.putUChar("cat_gap",    g_uiCfg.cat_gap);
    p.putUChar("list_item_h",g_uiCfg.list_item_h);
    p.putUChar("btn_radius", g_uiCfg.btn_radius);
    p.putUChar("tbtn_marg",  g_uiCfg.tbtn_margin);
    p.putUChar("list_rad",   g_uiCfg.list_radius);
    p.putUChar("sub_hdr_h",  g_uiCfg.sub_hdr_h);
    p.putUChar("cat_hdr_h",  g_uiCfg.cat_hdr_h);
    p.putUChar("pwr_save_m", g_uiCfg.power_save_min);
    p.putUChar("date_fmt",   g_uiCfg.date_format);
    p.putUChar("show_clock", g_uiCfg.show_clock);
    p.putUChar("sound_ok",   g_uiCfg.sound_ok);
    p.putUChar("sound_err",  g_uiCfg.sound_err);
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
