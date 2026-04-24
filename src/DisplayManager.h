#pragma once
#include <Arduino_GFX_Library.h>
#include "config.h"
#include "FoodAPI.h"

enum DateField { FIELD_DAY = 0, FIELD_MONTH, FIELD_YEAR, FIELD_DONE };

struct DateInput {
    int day, month, year;
    DateField activeField;
};

// ── Touch-Button-Positionen (für Hit-Tests in main.cpp) ──────────
// Standard: zentriert, 260px breit
static constexpr int16_t TBTN_X  = 10;
static constexpr int16_t TBTN_W  = 260;
static constexpr int16_t TBTN_H  = 52;

// Zwei Standard-Buttons übereinander am unteren Bildschirmrand
static constexpr int16_t TBTN_PRIMARY_Y   = 348;
static constexpr int16_t TBTN_SECONDARY_Y = 408;

// Datumseingabe: 3 Spalten (Tag=0, Mon=1, Jahr=2)
static constexpr int16_t DATE_COL_W  = DISPLAY_W / 3;   // 93px
static constexpr int16_t DATE_PLUS_Y0  = 90;
static constexpr int16_t DATE_PLUS_Y1  = 165;
static constexpr int16_t DATE_VAL_Y0   = 165;
static constexpr int16_t DATE_VAL_Y1   = 245;
static constexpr int16_t DATE_MINUS_Y0 = 245;
static constexpr int16_t DATE_MINUS_Y1 = 320;
// OK und Zurück bei Datum
static constexpr int16_t DATE_OK_Y     = 330;
static constexpr int16_t DATE_BACK_Y   = 396;

// Inventar-Browser Buttons
static constexpr int16_t INV_DEL_Y    = 355;
static constexpr int16_t INV_BACK_Y   = 415;

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void setBrightness(uint8_t val);

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

    void textCenter(const String &s, int16_t cx, int16_t cy, uint8_t sz,
                    uint16_t color, uint16_t bg = COLOR_BG);
    void textLeft(const String &s, int16_t x, int16_t y, uint8_t sz,
                  uint16_t color, uint16_t bg = COLOR_BG);
    int16_t textWidth(const String &s, uint8_t sz);

    void drawHeader(const String &title, uint16_t bgColor = COLOR_HEADER);
    void drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                         const String &label, uint16_t bg, uint16_t fg,
                         uint8_t textSz = 2);
    void drawPlusMinusColumn(int col, int value, bool isYear,
                             bool plusActive, bool minusActive);
    String daysLabel(int days);
};
