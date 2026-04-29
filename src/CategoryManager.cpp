#include "CategoryManager.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Definition des globalen Kategorienvektors
std::vector<CategoryDef> g_categories;

// Definition der globalen CategoryManager-Instanz
CategoryManager categoryManager;

// ── Default-Kategorien (Gefrierschrank-optimiert) ─────────────

void CategoryManager::loadDefaults() {
    g_categories.clear();
    struct { const char *name; uint16_t bg; uint16_t fg; } DEFAULTS[] = {
        { "Fleisch",    0x8000, 0xFFFF },
        { "Gefluegel",  0x7220, 0xFFFF },
        { "Fisch",      0x024F, 0xFFFF },
        { "Gemuese",    0x0280, 0xFFFF },
        { "Obst",       0xFBA0, 0x0841 },
        { "Fertig",     0x500C, 0xFFFF },
        { "Backwaren",  0xC540, 0x3180 },
        { "Sonstiges",  0x2945, 0xFFFF },
    };
    for (auto &d : DEFAULTS)
        g_categories.push_back({ d.name, d.bg, d.fg });
}

// ── Laden aus LittleFS ────────────────────────────────────────

bool CategoryManager::begin() {
    if (!LittleFS.exists(CATEGORIES_FILE)) {
        loadDefaults();
        return save();  // Defaults persistieren
    }
    File f = LittleFS.open(CATEGORIES_FILE, "r");
    if (!f) { loadDefaults(); return false; }

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close(); loadDefaults(); return false;
    }
    f.close();

    g_categories.clear();
    for (JsonObject obj : doc["cats"].as<JsonArray>()) {
        CategoryDef c;
        c.name      = obj["name"].as<String>();
        c.bgColor   = obj["bg"].as<uint16_t>();
        c.textColor = obj["fg"].as<uint16_t>();
        if (!c.name.isEmpty()) g_categories.push_back(c);
    }
    if (g_categories.empty()) loadDefaults();
    Serial.printf("[Cat] %d Kategorien geladen\n", (int)g_categories.size());
    return true;
}

// ── Speichern in LittleFS ─────────────────────────────────────

bool CategoryManager::save() {
    File f = LittleFS.open(CATEGORIES_FILE, "w");
    if (!f) return false;

    JsonDocument doc;
    JsonArray arr = doc["cats"].to<JsonArray>();
    for (const auto &c : g_categories) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = c.name;
        obj["bg"]   = c.bgColor;
        obj["fg"]   = c.textColor;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}
