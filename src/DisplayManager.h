#pragma once
#include <Arduino_GFX_Library.h>
#include "config.h"
#include "FoodAPI.h"

enum DateField { FIELD_DAY = 0, FIELD_MONTH, FIELD_YEAR, FIELD_DONE };

struct DateInput {
    int day, month, year;
    DateField activeField;
};

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void setBrightness(uint8_t val); // 0–255

    void showBooting(const String &msg = "");
    void showWifiConnecting(const String &ssid, int attempt = 0);
    void showIdle(int totalItems, int expiringItems, int expiredItems);
    void showScanning();
    void showFetching(const String &barcode);
    void showProduct(const ProductInfo &info, int stock);
    void showDateEntry(const DateInput &date, const String &productName);
    void showSuccess(const String &productName, const String &date);
    void showError(const String &msg);
    void showInventoryItem(int index, int total, const String &name,
                           const String &expiry, int qty, int daysLeft);

private:
    Arduino_DataBus *_bus;
    Arduino_GFX     *_gfx;

    // Text-Helfer
    void textCenter(const String &s, int16_t cx, int16_t cy, uint8_t sz, uint16_t color, uint16_t bg = COLOR_BG);
    void textLeft(const String &s, int16_t x, int16_t y, uint8_t sz, uint16_t color, uint16_t bg = COLOR_BG);
    int16_t textWidth(const String &s, uint8_t sz);

    void drawHeader(const String &title, uint16_t bgColor = COLOR_HEADER);
    void drawFooter(const String &left, const String &right = "");
    void drawStatusBar(int daysLeft); // farbiger Streifen oben am Artikel
    String daysLabel(int days);
};
