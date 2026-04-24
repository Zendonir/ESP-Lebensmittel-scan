#pragma once
#include <TFT_eSPI.h>
#include "config.h"
#include "FoodAPI.h"

// Datum-Eingabe-Felder
enum DateField { FIELD_DAY = 0, FIELD_MONTH, FIELD_YEAR, FIELD_DONE };

struct DateInput {
    int day, month, year;
    DateField activeField;
};

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void setBrightness(uint8_t pct); // 0–100

    // Bildschirme
    void showBooting(const String &msg = "");
    void showWifiConnecting(const String &ssid);
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
    TFT_eSPI _tft;
    void drawHeader(const String &title, uint16_t color = COLOR_HEADER);
    void drawFooter(const String &left, const String &right = "");
    void drawCentered(const String &text, int y, uint8_t size, uint16_t color);
    String daysLeftLabel(int days);
};
