#pragma once
#include <Arduino.h>
#include <vector>

struct CategoryDef {
    String   name;
    uint16_t bgColor;
    uint16_t textColor;
};

// Globaler dynamischer Kategorien-Vektor – gefüllt von CategoryManager::begin()
extern std::vector<CategoryDef> g_categories;
