#include "FoodAPI.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

ProductInfo FoodAPI::lookup(const String &barcode) {
    ProductInfo info;
    info.found = false;
    info.barcode = barcode;

    String url = "https://";
    url += OFF_HOST;
    url += "/api/v2/product/";
    url += barcode;
    url += "?fields=product_name,product_name_de,brands,quantity,categories_tags";

    String body = httpGet(url);
    if (body.isEmpty()) return info;

    // Nur benötigte Felder parsen – spart RAM
    JsonDocument filter;
    filter["status"] = true;
    filter["product"]["product_name_de"] = true;
    filter["product"]["product_name"] = true;
    filter["product"]["brands"] = true;
    filter["product"]["quantity"] = true;
    filter["product"]["categories_tags"][0] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err || doc["status"].as<int>() != 1) return info;

    info.found = true;

    // Deutschen Namen bevorzugen
    if (doc["product"]["product_name_de"].as<String>().length() > 0) {
        info.name = doc["product"]["product_name_de"].as<String>();
    } else {
        info.name = doc["product"]["product_name"].as<String>();
    }

    info.brand    = doc["product"]["brands"].as<String>();
    info.quantity = doc["product"]["quantity"].as<String>();

    // Erste Kategorie (Format: "en:dairy" → "dairy")
    String cat = doc["product"]["categories_tags"][0].as<String>();
    int colon = cat.indexOf(':');
    info.category = (colon >= 0) ? cat.substring(colon + 1) : cat;

    return info;
}

String FoodAPI::httpGet(const String &url) {
    WiFiClientSecure client;
    client.setInsecure(); // Kein Zertifikats-Check – für IoT ausreichend

    HTTPClient https;
    https.setUserAgent(OFF_USER_AGENT);
    https.setTimeout(10000);

    if (!https.begin(client, url)) return "";

    int code = https.GET();
    String body = "";
    if (code == HTTP_CODE_OK) {
        body = https.getString();
    }
    https.end();
    return body;
}
