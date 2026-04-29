#pragma once
#include "Categories.h"

// 12 voreingestellte Farben (RGB565 + HEX für Web-UI)
struct ColorPreset {
    const char *label;
    uint16_t    bg;
    uint16_t    fg;
    const char *hex;  // CSS-Farbe für Web-UI
};

static const ColorPreset COLOR_PRESETS[] = {
    { "Dunkelrot",   0x8000, 0xFFFF, "#800000" },
    { "Rotbraun",    0x7220, 0xFFFF, "#702000" },
    { "Dunkelblau",  0x024F, 0xFFFF, "#004080" },
    { "Dunkelgruen", 0x0280, 0xFFFF, "#005000" },
    { "Orange",      0xFBA0, 0x0841, "#F07000" },
    { "Violett",     0x500C, 0xFFFF, "#500060" },
    { "Gold",        0xC540, 0x3180, "#C08000" },
    { "Blaugrau",    0x2945, 0xFFFF, "#284048" },
    { "Teal",        0x0640, 0xFFFF, "#00C080" },
    { "Hellblau",    0x051F, 0xFFFF, "#004FC0" },
    { "Braun",       0x5300, 0xFFFF, "#502000" },
    { "Grau",        0x4A49, 0xFFFF, "#484848" },
};
static constexpr int NUM_COLOR_PRESETS = 12;

class CategoryManager {
public:
    bool begin();   // lädt aus LittleFS, fällt auf Defaults zurück
    bool save();
    int  count() const { return (int)g_categories.size(); }

private:
    void loadDefaults();
};

extern CategoryManager categoryManager;
