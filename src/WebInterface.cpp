#include "WebInterface.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

WebInterface::WebInterface(Inventory &inventory)
    : _server(80), _inventory(inventory) {}

void WebInterface::begin() {
    // Statische Dateien aus LittleFS (index.html etc.)
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // GET /api/inventory – alle Artikel als JSON
    _server.on("/api/inventory", HTTP_GET, [this](AsyncWebServerRequest *req) {
        handleInventoryAPI(req);
    });

    // DELETE /api/item?barcode=xxx&expiry=YYYY-MM-DD
    _server.on("/api/item", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        handleDeleteAPI(req);
    });

    // GET /api/stats
    _server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *req) {
        handleStatsAPI(req);
    });

    _server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    _server.begin();
    Serial.println("[Web] Server gestartet auf Port 80");
}

void WebInterface::handleInventoryAPI(AsyncWebServerRequest *request) {
    // Sortierung: nach Ablaufdatum
    auto items = _inventory.items(); // Kopie
    std::sort(items.begin(), items.end(), [](const InventoryItem &a, const InventoryItem &b) {
        return a.expiryDate < b.expiryDate;
    });

    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    time_t now = time(nullptr);

    for (const auto &item : items) {
        JsonObject obj = arr.add<JsonObject>();
        obj["barcode"] = item.barcode;
        obj["name"]    = item.name;
        obj["brand"]   = item.brand;
        obj["expiry"]  = item.expiryDate;
        obj["added"]   = item.addedDate;
        obj["qty"]     = item.quantity;

        // Tage bis Ablauf berechnen
        struct tm tm = {};
        if (item.expiryDate.length() >= 10) {
            tm.tm_year = item.expiryDate.substring(0, 4).toInt() - 1900;
            tm.tm_mon  = item.expiryDate.substring(5, 7).toInt() - 1;
            tm.tm_mday = item.expiryDate.substring(8, 10).toInt();
            obj["daysLeft"] = (int)(difftime(mktime(&tm), now) / 86400.0);
        }
    }

    doc["count"] = items.size();

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebInterface::handleDeleteAPI(AsyncWebServerRequest *request) {
    if (!request->hasParam("barcode") || !request->hasParam("expiry")) {
        request->send(400, "application/json", "{\"error\":\"Fehlende Parameter\"}");
        return;
    }
    String barcode = request->getParam("barcode")->value();
    String expiry  = request->getParam("expiry")->value();

    if (_inventory.removeItem(barcode, expiry)) {
        request->send(200, "application/json", "{\"ok\":true}");
    } else {
        request->send(404, "application/json", "{\"error\":\"Artikel nicht gefunden\"}");
    }
}

void WebInterface::handleStatsAPI(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["total"]    = _inventory.count();
    doc["expiring"] = _inventory.expiringIn(WARNING_DAYS);
    doc["expired"]  = _inventory.expiredCount();

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}
