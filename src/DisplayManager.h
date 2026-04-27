#pragma once
#include <Arduino_GFX_Library.h>
#include "config.h"
#include "FoodAPI.h"
#include "CustomProducts.h"
#include "Categories.h"

enum DateField { FIELD_DAY = 0, FIELD_MONTH, FIELD_YEAR, FIELD_DONE };

struct DateInput {
    int day, month, year;
    DateField activeField;
};

// ── Touch-Button-Konstanten ──────────────────────────────────

// Universelle Buttons
static constexpr int16_t TBTN_X           = 10;
static constexpr int16_t TBTN_W           = 260;
static constexpr int16_t TBTN_H           = 52;
static constexpr int16_t TBTN_PRIMARY_Y   = 348;
static constexpr int16_t TBTN_SECONDARY_Y = 408;

// IDLE-Screen
static constexpr int16_t IDLE_LIST_BTN_Y = 300;
static constexpr int16_t IDLE_INV_BTN_Y  = 364;
static constexpr int16_t IDLE_BTN_H      = 56;

// Datumseingabe
static constexpr int16_t DATE_COL_W    = DISPLAY_W / 3;
static constexpr int16_t DATE_PLUS_Y0  = 90;
static constexpr int16_t DATE_PLUS_Y1  = 165;
static constexpr int16_t DATE_VAL_Y0   = 165;
static constexpr int16_t DATE_VAL_Y1   = 245;
static constexpr int16_t DATE_MINUS_Y0 = 245;
static constexpr int16_t DATE_MINUS_Y1 = 320;
static constexpr int16_t DATE_OK_Y     = 330;
static constexpr int16_t DATE_BACK_Y   = 396;

// Kategorien-Grid (2 Spalten × 4 Zeilen)
static constexpr int16_t CAT_X0      = 8;
static constexpr int16_t CAT_Y0      = 64;    // nach Header (56px) + 8px
static constexpr int16_t CAT_BTN_W   = 128;
static constexpr int16_t CAT_BTN_H   = 88;
static constexpr int16_t CAT_GAP     = 8;

// Produktliste
static constexpr int16_t LIST_ITEM_H     = 50;
static constexpr int16_t LIST_MAX_VIS    = 7;
static constexpr int16_t LIST_BACK_BTN_Y = DISPLAY_H - 44;

// Inventar-Browser
static constexpr int16_t INV_DEL_Y    = 355;
static constexpr int16_t INV_BACK_Y   = 415;

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void setBrightness(uint8_t val);

    void showBooting(const String &msg = "");
    void showWifiConnecting(const String &ssid, int attempt = 0);
    void showIdle(int totalItems, int expiringItems, int expiredItems,
                  int customCount = 0);
    void showScanning();
    void showFetching(const String &barcode);
    void showProduct(const ProductInfo &info, int stock);
    void showCategorySelect();
    void showProductList(const std::vector<CustomProduct> &products,
                         int offset, const String &category);
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
    void drawPlusMinusColumn(int col, int value, bool isYear);
    void drawCategoryIcon(uint8_t catIndex, int16_t cx, int16_t cy);
    String daysLabel(int days);
};
