#include "ShoppingList.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <time.h>

ShoppingList shoppingList;

bool ShoppingList::begin() { return load(); }

bool ShoppingList::add(const String &name, const String &category) {
    // Nicht doppelt eintragen wenn noch nicht gekauft
    for (const auto &it : _items)
        if (it.name == name && !it.bought) return true;
    if ((int)_items.size() >= 100) _items.erase(_items.begin());
    time_t now = time(nullptr); struct tm *t = localtime(&now);
    char buf[12]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    _items.push_back({ name, category, String(buf), false });
    return save();
}

bool ShoppingList::markBought(int index) {
    if (index < 0 || index >= (int)_items.size()) return false;
    _items[index].bought = !_items[index].bought;  // Toggle
    return save();
}

bool ShoppingList::remove(int index) {
    if (index < 0 || index >= (int)_items.size()) return false;
    _items.erase(_items.begin() + index);
    return save();
}

bool ShoppingList::clearBought() {
    _items.erase(std::remove_if(_items.begin(), _items.end(),
        [](const ShoppingItem &i) { return i.bought; }), _items.end());
    return save();
}

bool ShoppingList::load() {
    _items.clear();
    if (!LittleFS.exists(SHOPPING_LIST_FILE)) return true;
    File f = LittleFS.open(SHOPPING_LIST_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    for (JsonObject o : doc["items"].as<JsonArray>()) {
        _items.push_back({
            o["name"].as<String>(),
            o["cat"].as<String>(),
            o["date"].as<String>(),
            o["bought"] | false
        });
    }
    return true;
}

bool ShoppingList::save() {
    File f = LittleFS.open(SHOPPING_LIST_FILE, "w");
    if (!f) return false;
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (const auto &it : _items) {
        JsonObject o = arr.add<JsonObject>();
        o["name"]   = it.name;
        o["cat"]    = it.category;
        o["date"]   = it.addedDate;
        o["bought"] = it.bought;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}
