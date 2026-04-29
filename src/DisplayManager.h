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

// ── Hauptscreen (Kategorietabs + Produktliste) ───────────────
static constexpr int16_t TABS_H        = 34;
static constexpr int16_t TABS_TAB_W    = DISPLAY_W / 8;    // 57px
static constexpr int16_t MAIN_LIST_Y   = TABS_H;
static constexpr int16_t MAIN_ITEM_H   = 44;
static constexpr int16_t MAIN_MAX_VIS  = 5;                 // 5×44=220px → y=34..254
static constexpr int16_t MAIN_HINT_Y   = MAIN_LIST_Y + MAIN_MAX_VIS * MAIN_ITEM_H; // 254
static constexpr int16_t MAIN_INV_X    = DISPLAY_W - 112;  // Inventar-Button rechts
static constexpr int16_t MAIN_INV_W    = 112;

// ── Universelle Buttons (landscape 456×280) ──────────────────
static constexpr int16_t TBTN_X           = 8;
static constexpr int16_t TBTN_W           = 440;
static constexpr int16_t TBTN_H           = 44;
static constexpr int16_t TBTN_PRIMARY_Y   = 228;
static constexpr int16_t TBTN_SECONDARY_Y = 176;

// ── IDLE-Buttons (AP-Mode) ───────────────────────────────────
static constexpr int16_t IDLE_LIST_BTN_Y = 172;
static constexpr int16_t IDLE_INV_BTN_Y  = 220;
static constexpr int16_t IDLE_BTN_H      = 40;

// ── Datumseingabe ────────────────────────────────────────────
static constexpr int16_t DATE_COL_W    = DISPLAY_W / 3;    // 152
static constexpr int16_t DATE_PLUS_Y0  = 58;
static constexpr int16_t DATE_PLUS_Y1  = 114;
static constexpr int16_t DATE_VAL_Y0   = 116;
static constexpr int16_t DATE_VAL_Y1   = 190;
static constexpr int16_t DATE_MINUS_Y0 = 192;
static constexpr int16_t DATE_MINUS_Y1 = 238;
static constexpr int16_t DATE_BTN_Y    = 242;
static constexpr int16_t DATE_BTN_H    = 34;
static constexpr int16_t DATE_BACK_X   = 8;
static constexpr int16_t DATE_BACK_W   = 140;
static constexpr int16_t DATE_OK_X     = 156;
static constexpr int16_t DATE_OK_W     = 292;

// ── Kategorien-Grid (für den alten showCategorySelect – nicht mehr Hauptscreen) ─
static constexpr int16_t CAT_X0      = 4;
static constexpr int16_t CAT_Y0      = 44;
static constexpr int16_t CAT_BTN_W   = 106;
static constexpr int16_t CAT_BTN_H   = 106;
static constexpr int16_t CAT_GAP     = 8;

// ── Produktliste ─────────────────────────────────────────────
static constexpr int16_t LIST_ITEMS_Y    = 40;
static constexpr int16_t LIST_ITEM_H     = 44;
static constexpr int16_t LIST_MAX_VIS    = 4;
static constexpr int16_t LIST_BACK_BTN_Y = 220;

// ── Inventar-Browser ─────────────────────────────────────────
static constexpr int16_t INV_DEL_Y  = 228;
static constexpr int16_t INV_BACK_Y = 176;

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void setBrightness(uint8_t val);

    // Hauptscreen: Kategorietabs + Produktliste der aktiven Kategorie
    // catInvCounts: Inventar-Anzahl je Kategorie; warnCount: bald ablaufende Artikel
    void showMain(int catIndex, const std::vector<CustomProduct> &products, int offset,
                  const std::vector<int> &catInvCounts, int warnCount, bool wifiOk);

    void showBooting(const String &msg = "");
    void showWifiConnecting(const String &ssid, int attempt = 0);
    void showScanning();
    void showFetching(const String &barcode);
    void showDateEntry(const DateInput &date, const String &productName);
    void showPrinting();
    // showSuccess: showReprint=true → zeigt "Nochmal drucken"-Button
    void showSuccess(const String &productName, const String &date, bool showReprint = true);
    void showError(const String &msg);
    void showRetrieve(const String &name, const String &storageDate,
                      const String &expiryDate, int daysLeft);
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
