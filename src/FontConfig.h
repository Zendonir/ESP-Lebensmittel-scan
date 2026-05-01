#pragma once
#include <stdint.h>
#include <Preferences.h>

// Alle Werte in Pixelhöhe (Vielfaches von 8: 8, 16, 24, 32).
// pxToSz() wandelt in den GFX-Faktor um (px / 8, begrenzt auf 1–4).
struct FontConfig {
    uint8_t title;  // Überschriften / Screen-Titel      (default 16 px)
    uint8_t body;   // Produktnamen / Haupttext           (default 16 px)
    uint8_t small;  // Labels, Uhrzeit, Hinweistexte      (default  8 px)
    uint8_t value;  // Hervorgehobene Zahlen (Tage, Drum) (default 24 px)
    uint8_t key;    // Ziffernblock-Tasten                (default 24 px)
    uint8_t btn;    // Button-Beschriftungen              (default 16 px)
};

extern FontConfig g_fontCfg;

inline uint8_t pxToSz(uint8_t px) {
    uint8_t s = px / 8;
    return s < 1 ? 1 : s > 4 ? 4 : s;
}

inline void loadFontConfig() {
    Preferences p; p.begin("fonts", true);
    g_fontCfg.title = p.getUChar("title", 16);
    g_fontCfg.body  = p.getUChar("body",  16);
    g_fontCfg.small = p.getUChar("small",  8);
    g_fontCfg.value = p.getUChar("value", 24);
    g_fontCfg.key   = p.getUChar("key",   24);
    g_fontCfg.btn   = p.getUChar("btn",   16);
    p.end();
    // Pixelwerte auf 8–32 begrenzen
    auto clamp = [](uint8_t v) -> uint8_t { return v < 8 ? 8 : v > 32 ? 32 : v; };
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
