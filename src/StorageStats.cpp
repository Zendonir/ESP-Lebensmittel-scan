#include "StorageStats.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

StorageStats storageStats;

bool StorageStats::begin() { return load(); }

bool StorageStats::record(const InventoryItem &item, const String &removedDate, int storageDays) {
    if ((int)_items.size() >= MAX_STATS_ENTRIES)
        _items.erase(_items.begin());
    _items.push_back({ item.name, item.category, item.addedDate, removedDate, storageDays });
    return save();
}

bool StorageStats::load() {
    _items.clear();
    if (!LittleFS.exists(STORAGE_STATS_FILE)) return true;
    File f = LittleFS.open(STORAGE_STATS_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    for (JsonObject o : doc["stats"].as<JsonArray>()) {
        _items.push_back({
            o["name"].as<String>(),
            o["cat"].as<String>(),
            o["added"].as<String>(),
            o["removed"].as<String>(),
            o["days"] | 0
        });
    }
    return true;
}

bool StorageStats::save() {
    File f = LittleFS.open(STORAGE_STATS_FILE, "w");
    if (!f) return false;
    JsonDocument doc;
    JsonArray arr = doc["stats"].to<JsonArray>();
    for (const auto &s : _items) {
        JsonObject o = arr.add<JsonObject>();
        o["name"]    = s.name;
        o["cat"]     = s.category;
        o["added"]   = s.addedDate;
        o["removed"] = s.removedDate;
        o["days"]    = s.storageDays;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}
