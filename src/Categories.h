#pragma once
#include <Arduino.h>

// 8 Kategorien optimiert für Gefrierschrank-Einlagerung
// bgColor = RGB565 Hintergrundfarbe der Kachel
// textColor = Schriftfarbe auf der Kachel
struct CategoryDef {
    const char *name;       // Anzeigename (und Speicherschlüssel)
    uint16_t    bgColor;
    uint16_t    textColor;
};

static const CategoryDef CATEGORIES[8] = {
    { "Fleisch",    0x8000, 0xFFFF },   // dunkelrot
    { "Gefluegel",  0x7220, 0xFFFF },   // braun
    { "Fisch",      0x024F, 0xFFFF },   // ozeanblau
    { "Gemuese",    0x0280, 0xFFFF },   // dunkelgrün
    { "Obst",       0xFBA0, 0x0841 },   // orange, dunkle Schrift
    { "Fertig",     0x500C, 0xFFFF },   // dunkelviolett
    { "Backwaren",  0xC540, 0x3180 },   // goldgelb, dunkle Schrift
    { "Sonstiges",  0x2945, 0xFFFF },   // blaugrau
};
static constexpr int NUM_CATEGORIES = 8;
