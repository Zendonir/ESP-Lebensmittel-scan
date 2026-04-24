#include "WebInterface.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

WebInterface::WebInterface(Inventory &inventory, CustomProducts &customProducts)
    : _server(80), _inventory(inventory), _customProducts(customProducts) {}

void WebInterface::begin() {
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ── Inventar ─────────────────────────────────────────────

    _server.on("/api/inventory", HTTP_GET, [this](AsyncWebServerRequest *req) {
        auto items = _inventory.items();
        std::sort(items.begin(), items.end(), [](const InventoryItem &a, const InventoryItem &b) {
            return a.expiryDate < b.expiryDate;
        });
        JsonDocument doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        time_t now = time(nullptr);
        for (const auto &it : items) {
            JsonObject obj = arr.add<JsonObject>();
            obj["barcode"] = it.barcode;
            obj["name"]    = it.name;
            obj["brand"]   = it.brand;
            obj["expiry"]  = it.expiryDate;
            obj["added"]   = it.addedDate;
            obj["qty"]     = it.quantity;
            if (it.expiryDate.length() >= 10) {
                struct tm tm = {};
                tm.tm_year = it.expiryDate.substring(0, 4).toInt() - 1900;
                tm.tm_mon  = it.expiryDate.substring(5, 7).toInt() - 1;
                tm.tm_mday = it.expiryDate.substring(8, 10).toInt();
                obj["daysLeft"] = (int)(difftime(mktime(&tm), now) / 86400.0);
            }
        }
        doc["count"] = items.size();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    _server.on("/api/item", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        if (!req->hasParam("barcode") || !req->hasParam("expiry")) {
            req->send(400, "application/json", "{\"error\":\"Fehlende Parameter\"}");
            return;
        }
        bool ok = _inventory.removeItem(req->getParam("barcode")->value(),
                                        req->getParam("expiry")->value());
        req->send(ok ? 200 : 404, "application/json",
                  ok ? "{\"ok\":true}" : "{\"error\":\"Nicht gefunden\"}");
    });

    _server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["total"]    = _inventory.count();
        doc["expiring"] = _inventory.expiringIn(WARNING_DAYS);
        doc["expired"]  = _inventory.expiredCount();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── Eigene Produkte (Vorlagen) ────────────────────────────

    _server.on("/api/custom-products", HTTP_GET, [this](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        for (const auto &p : _customProducts.items()) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"]    = p.name;
            obj["brand"]   = p.brand;
            obj["barcode"] = p.barcode;
        }
        doc["count"] = _customProducts.count();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // POST /api/custom-products   Body: {"name":"...","brand":"...","barcode":"..."}
    _server.on("/api/custom-products", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, (const char *)data, len)) {
                req->send(400, "application/json", "{\"error\":\"Ungültiges JSON\"}");
                return;
            }
            CustomProduct p;
            p.name    = doc["name"].as<String>();
            p.brand   = doc["brand"].as<String>();
            p.barcode = doc["barcode"].as<String>();
            p.name.trim();
            if (p.name.isEmpty()) {
                req->send(400, "application/json", "{\"error\":\"Name erforderlich\"}");
                return;
            }
            bool ok = _customProducts.add(p);
            req->send(ok ? 200 : 500, "application/json",
                      ok ? "{\"ok\":true}" : "{\"error\":\"Speichern fehlgeschlagen\"}");
        }
    );

    // DELETE /api/custom-products?index=N
    _server.on("/api/custom-products", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        if (!req->hasParam("index")) {
            req->send(400, "application/json", "{\"error\":\"index fehlt\"}");
            return;
        }
        int idx = req->getParam("index")->value().toInt();
        bool ok = _customProducts.remove(idx);
        req->send(ok ? 200 : 404, "application/json",
                  ok ? "{\"ok\":true}" : "{\"error\":\"Nicht gefunden\"}");
    });

    _server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    _server.begin();
    Serial.println("[Web] Server gestartet auf Port 80");
}
