#include "FoodCache.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

FoodCache foodCache;

bool FoodCache::begin() { return load(); }

ProductInfo FoodCache::get(const String &barcode) const {
    for (const auto &e : _entries) {
        if (e.barcode == barcode) {
            return { true, e.barcode, e.name, e.brand, e.quantity, e.category };
        }
    }
    return { false, barcode };
}

void FoodCache::put(const ProductInfo &p) {
    if (!p.found || p.barcode.isEmpty()) return;
    for (auto &e : _entries) {
        if (e.barcode == p.barcode) {
            e.name = p.name; e.brand = p.brand;
            e.quantity = p.quantity; e.category = p.category;
            save(); return;
        }
    }
    if ((int)_entries.size() >= MAX_CACHE_ENTRIES)
        _entries.erase(_entries.begin());
    _entries.push_back({ p.barcode, p.name, p.brand, p.quantity, p.category });
    save();
}

bool FoodCache::load() {
    _entries.clear();
    if (!LittleFS.exists(OFF_CACHE_FILE)) return true;
    File f = LittleFS.open(OFF_CACHE_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    for (JsonObject o : doc["items"].as<JsonArray>()) {
        _entries.push_back({
            o["barcode"].as<String>(),
            o["name"].as<String>(),
            o["brand"].as<String>(),
            o["quantity"].as<String>(),
            o["category"].as<String>()
        });
    }
    return true;
}

bool FoodCache::save() {
    File f = LittleFS.open(OFF_CACHE_FILE, "w");
    if (!f) return false;
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (const auto &e : _entries) {
        JsonObject o = arr.add<JsonObject>();
        o["barcode"]  = e.barcode;
        o["name"]     = e.name;
        o["brand"]    = e.brand;
        o["quantity"] = e.quantity;
        o["category"] = e.category;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}
