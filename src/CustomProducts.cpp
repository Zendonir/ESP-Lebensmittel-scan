#include "CustomProducts.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool CustomProducts::begin() { return load(); }

bool CustomProducts::load() {
    _items.clear();
    if (!LittleFS.exists(CUSTOM_PRODUCTS_FILE)) return true;

    File f = LittleFS.open(CUSTOM_PRODUCTS_FILE, "r");
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();

    for (JsonObject obj : doc["products"].as<JsonArray>()) {
        CustomProduct p;
        p.name        = obj["name"].as<String>();
        p.brand       = obj["brand"].as<String>();
        p.barcode     = obj["barcode"].as<String>();
        p.category    = obj["category"].as<String>();
        p.description = obj["description"].as<String>();
        p.defaultDays = obj["defaultDays"] | 0;
        p.askQty      = obj["askQty"] | false;
        if (p.category.isEmpty()) p.category = "Sonstiges";
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
        obj["name"]        = p.name;
        obj["brand"]       = p.brand;
        obj["barcode"]     = p.barcode;
        obj["category"]    = p.category;
        obj["description"] = p.description;
        obj["defaultDays"] = p.defaultDays;
        obj["askQty"]      = p.askQty;
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

std::vector<CustomProduct> CustomProducts::byCategory(const String &cat) const {
    std::vector<CustomProduct> result;
    for (const auto &p : _items)
        if (p.category == cat) result.push_back(p);
    return result;
}
