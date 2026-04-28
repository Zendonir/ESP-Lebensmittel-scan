#include "Inventory.h"
#include "config.h"
#include <LittleFS.h>
#include <time.h>

bool Inventory::begin() {
    return load();
}

bool Inventory::load() {
    _items.clear();
    if (!LittleFS.exists(INVENTORY_FILE)) return true; // leeres Inventar ist ok

    File f = LittleFS.open(INVENTORY_FILE, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[Inventory] JSON parse error: %s\n", err.c_str());
        return false;
    }

    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        InventoryItem item;
        item.barcode    = obj["barcode"].as<String>();
        item.name       = obj["name"].as<String>();
        item.brand      = obj["brand"].as<String>();
        item.expiryDate = obj["expiry"].as<String>();
        item.addedDate  = obj["added"].as<String>();
        item.quantity   = obj["qty"].as<int>();
        _items.push_back(item);
    }
    return true;
}

bool Inventory::save() {
    File f = LittleFS.open(INVENTORY_FILE, "w");
    if (!f) return false;

    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (const auto &item : _items) {
        JsonObject obj = arr.add<JsonObject>();
        obj["barcode"] = item.barcode;
        obj["name"]    = item.name;
        obj["brand"]   = item.brand;
        obj["expiry"]  = item.expiryDate;
        obj["added"]   = item.addedDate;
        obj["qty"]     = item.quantity;
    }

    serializeJson(doc, f);
    f.close();
    return true;
}

bool Inventory::addItem(const InventoryItem &item) {
    // Gleicher Barcode + Ablaufdatum → Menge erhöhen
    for (auto &existing : _items) {
        if (existing.barcode == item.barcode && existing.expiryDate == item.expiryDate) {
            existing.quantity += item.quantity;
            return save();
        }
    }
    if ((int)_items.size() >= MAX_ITEMS) return false;
    _items.push_back(item);
    return save();
}

bool Inventory::removeItem(const String &barcode, const String &expiryDate) {
    for (auto it = _items.begin(); it != _items.end(); ++it) {
        if (it->barcode == barcode && it->expiryDate == expiryDate) {
            _items.erase(it);
            return save();
        }
    }
    return false;
}

bool Inventory::incrementItem(const String &barcode, const String &expiryDate) {
    for (auto &item : _items) {
        if (item.barcode == barcode && item.expiryDate == expiryDate) {
            item.quantity++;
            return save();
        }
    }
    return false;
}

int Inventory::daysUntilExpiry(const String &dateStr) const {
    // dateStr Format: "YYYY-MM-DD"
    if (dateStr.length() < 10) return 9999;

    struct tm expiry = {};
    expiry.tm_year = dateStr.substring(0, 4).toInt() - 1900;
    expiry.tm_mon  = dateStr.substring(5, 7).toInt() - 1;
    expiry.tm_mday = dateStr.substring(8, 10).toInt();

    time_t now = time(nullptr);
    time_t exp = mktime(&expiry);
    double diff = difftime(exp, now);
    return (int)(diff / 86400.0);
}

int Inventory::expiringIn(int days) const {
    int count = 0;
    for (const auto &item : _items) {
        int d = daysUntilExpiry(item.expiryDate);
        if (d >= 0 && d <= days) count += item.quantity;
    }
    return count;
}

int Inventory::expiredCount() const {
    int count = 0;
    for (const auto &item : _items) {
        if (daysUntilExpiry(item.expiryDate) < 0) count += item.quantity;
    }
    return count;
}
