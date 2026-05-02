#include "ServerSync.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

ServerSync serverSync;

bool ServerSync::begin(const Config &cfg) {
    _cfg = cfg;
    if (_cfg.deviceId.isEmpty())
        _cfg.deviceId = WiFi.macAddress();
    return true;
}

void ServerSync::loop() {
    if (!isConfigured()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (!_registered) {
        if (millis() - _lastRetry > 10000) {
            _lastRetry = millis();
            _registered = registerDevice();
        }
        return;
    }
    if (!_queue.empty() && millis() - _lastRetry > 5000) {
        _lastRetry = millis();
        processQueue();
    }
}

void ServerSync::onItemAdded(const InventoryItem &item) {
    if (!isConfigured()) return;
    _queue.push_back({ "item_added", buildItemPayload("item_added", item) });
    if ((int)_queue.size() > 50) _queue.erase(_queue.begin());
}

void ServerSync::onItemRemoved(const InventoryItem &item, int storageDays) {
    if (!isConfigured()) return;
    _queue.push_back({ "item_removed", buildItemPayload("item_removed", item, storageDays) });
    if ((int)_queue.size() > 50) _queue.erase(_queue.begin());
}

String ServerSync::buildItemPayload(const String &type, const InventoryItem &item, int storageDays) {
    JsonDocument doc;
    doc["device_id"]   = _cfg.deviceId;
    doc["device_name"] = _cfg.deviceName;
    doc["device_room"] = _cfg.deviceRoom;
    doc["type"]        = type;
    JsonObject d       = doc["data"].to<JsonObject>();
    d["name"]          = item.name;
    d["brand"]         = item.brand;
    d["category"]      = item.category;
    d["expiry"]        = item.expiryDate;
    d["added"]         = item.addedDate;
    d["qty"]           = item.quantity;
    d["label"]         = item.labelBarcode;
    if (storageDays >= 0) d["storageDays"] = storageDays;
    String out; serializeJson(doc, out);
    return out;
}

bool ServerSync::registerDevice() {
    JsonDocument doc;
    doc["device_id"]    = _cfg.deviceId;
    doc["device_name"]  = _cfg.deviceName;
    doc["device_room"]  = _cfg.deviceRoom;
    doc["ip"]           = WiFi.localIP().toString();
    if (_cfg.householdId > 0)
        doc["household_id"] = _cfg.householdId;
    String body; serializeJson(doc, body);
    return httpPost("/api/devices", body);
}

void ServerSync::processQueue() {
    while (!_queue.empty()) {
        if (!httpPost("/api/events", _queue.front().payload)) break;
        _queue.erase(_queue.begin());
    }
}

bool ServerSync::httpPost(const String &path, const String &body) {
    HTTPClient http;
    http.setTimeout(3000);
    if (!http.begin(_cfg.serverUrl + path)) return false;
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();
    return code >= 200 && code < 300;
}

ServerSync::Config ServerSync::loadConfig() {
    Config c;
    Preferences p; p.begin("sync", true);
    c.serverUrl   = p.getString("url",    "");
    c.deviceName  = p.getString("name",   "Lebensmittel Scanner");
    c.deviceRoom  = p.getString("room",   "");
    c.householdId = p.getInt("household", 0);
    c.deviceId    = WiFi.macAddress();
    p.end();
    return c;
}

void ServerSync::saveConfig(const Config &cfg) {
    Preferences p; p.begin("sync", false);
    p.putString("url",      cfg.serverUrl);
    p.putString("name",     cfg.deviceName);
    p.putString("room",     cfg.deviceRoom);
    p.putInt("household",   cfg.householdId);
    p.end();
}
