#pragma once
#include <stdint.h>
#include <Preferences.h>

struct FontConfig {
    uint8_t title;  // Überschriften / Screen-Titel
    uint8_t body;   // Produktnamen / Haupttext
    uint8_t small;  // Labels, Uhrzeit, Hinweise
    uint8_t value;  // Hervorgehobene Zahlen (Tage, Drum-Auswahl)
    uint8_t key;    // Ziffernblock-Tasten
    uint8_t btn;    // Button-Beschriftungen
};

extern FontConfig g_fontCfg;

inline void loadFontConfig() {
    Preferences p; p.begin("fonts", true);
    g_fontCfg.title = p.getUChar("title", 2);
    g_fontCfg.body  = p.getUChar("body",  2);
    g_fontCfg.small = p.getUChar("small", 1);
    g_fontCfg.value = p.getUChar("value", 3);
    g_fontCfg.key   = p.getUChar("key",   3);
    g_fontCfg.btn   = p.getUChar("btn",   2);
    p.end();
    // Werte auf 1–4 begrenzen
    auto clamp = [](uint8_t v) -> uint8_t { return v < 1 ? 1 : v > 4 ? 4 : v; };
    g_fontCfg.title = clamp(g_fontCfg.title);
    g_fontCfg.body  = clamp(g_fontCfg.body);
    g_fontCfg.small = clamp(g_fontCfg.small);
    g_fontCfg.value = clamp(g_fontCfg.value);
    g_fontCfg.key   = clamp(g_fontCfg.key);
    g_fontCfg.btn   = clamp(g_fontCfg.btn);
}

inline void saveFontConfig(const FontConfig &cfg) {
    Preferences p; p.begin("fonts", false);
    p.putUChar("title", cfg.title);
    p.putUChar("body",  cfg.body);
    p.putUChar("small", cfg.small);
    p.putUChar("value", cfg.value);
    p.putUChar("key",   cfg.key);
    p.putUChar("btn",   cfg.btn);
    p.end();
}
