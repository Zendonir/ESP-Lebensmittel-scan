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

// ── Touch-Button-Konstanten (Landscape 456×280) ──────────────

// Universelle Buttons (volle Breite, am unteren Rand)
static constexpr int16_t TBTN_X           = 8;
static constexpr int16_t TBTN_W           = 440;
static constexpr int16_t TBTN_H           = 44;
static constexpr int16_t TBTN_PRIMARY_Y   = 228;   // unterster Button
static constexpr int16_t TBTN_SECONDARY_Y = 176;   // Button darüber (8px Abstand)

// IDLE-Screen
static constexpr int16_t IDLE_LIST_BTN_Y = 172;
static constexpr int16_t IDLE_INV_BTN_Y  = 220;
static constexpr int16_t IDLE_BTN_H      = 40;

// Datumseingabe – 3 Spalten à 152 px, große Touch-Flächen
static constexpr int16_t DATE_COL_W    = DISPLAY_W / 3;  // 152
static constexpr int16_t DATE_PLUS_Y0  = 58;
static constexpr int16_t DATE_PLUS_Y1  = 114;
static constexpr int16_t DATE_VAL_Y0   = 116;
static constexpr int16_t DATE_VAL_Y1   = 190;
static constexpr int16_t DATE_MINUS_Y0 = 192;
static constexpr int16_t DATE_MINUS_Y1 = 238;
// OK/Abbrechen: nebeneinander am unteren Rand
static constexpr int16_t DATE_BTN_Y    = 242;
static constexpr int16_t DATE_BTN_H    = 34;
static constexpr int16_t DATE_BACK_X   = 8;
static constexpr int16_t DATE_BACK_W   = 140;
static constexpr int16_t DATE_OK_X     = 156;
static constexpr int16_t DATE_OK_W     = 292;

// Kategorien-Grid (4 Spalten × 2 Zeilen)
static constexpr int16_t CAT_X0      = 4;
static constexpr int16_t CAT_Y0      = 44;
static constexpr int16_t CAT_BTN_W   = 106;
static constexpr int16_t CAT_BTN_H   = 106;
static constexpr int16_t CAT_GAP     = 8;

// Produktliste
static constexpr int16_t LIST_ITEMS_Y    = 40;
static constexpr int16_t LIST_ITEM_H     = 44;
static constexpr int16_t LIST_MAX_VIS    = 4;
static constexpr int16_t LIST_BACK_BTN_Y = 220;

// Inventar-Browser
static constexpr int16_t INV_DEL_Y  = 228;
static constexpr int16_t INV_BACK_Y = 176;

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
    void showAPMode(const String &ssid, const String &password, const String &ip);

private:
    Arduino_DataBus *_bus;
    Arduino_CO5300  *_panel;
    Arduino_Canvas  *_gfx;

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
