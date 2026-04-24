#include "CustomProducts.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool CustomProducts::begin() {
    return load();
}

bool CustomProducts::load() {
    _items.clear();
    if (!LittleFS.exists(CUSTOM_PRODUCTS_FILE)) return true;

    File f = LittleFS.open(CUSTOM_PRODUCTS_FILE, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    for (JsonObject obj : doc["products"].as<JsonArray>()) {
        CustomProduct p;
        p.name    = obj["name"].as<String>();
        p.brand   = obj["brand"].as<String>();
        p.barcode = obj["barcode"].as<String>();
        if (!p.name.isEmpty()) _items.push_back(p);
    }
    return true;
}

bool CustomProducts::save() {
    File f = LittleFS.open(CUSTOM_PRODUCTS_FILE, "w");
    if (!f) return false;

    JsonDocument doc;
    JsonArray arr = doc["products"].to<JsonArray>();
    for (const auto &p : _items) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"]    = p.name;
        obj["brand"]   = p.brand;
        obj["barcode"] = p.barcode;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

bool CustomProducts::add(const CustomProduct &p) {
    if (p.name.isEmpty()) return false;
    _items.push_back(p);
    return save();
}

bool CustomProducts::remove(int index) {
    if (index < 0 || index >= (int)_items.size()) return false;
    _items.erase(_items.begin() + index);
    return save();
}

bool CustomProducts::update(int index, const CustomProduct &p) {
    if (index < 0 || index >= (int)_items.size()) return false;
    _items[index] = p;
    return save();
}
