#pragma once
#include <Arduino_GFX_Library.h>
#include <vector>
#include "config.h"
#include "FoodAPI.h"
#include "CustomProducts.h"
#include "Categories.h"

enum DateField { FIELD_DAY = 0, FIELD_MONTH, FIELD_YEAR, FIELD_DONE };

struct DateInput {
    int day, month, year;
    DateField activeField;
};

// ── Hauptscreen (Kategorie-Grid) — Hochformat 280×456 ────────
static constexpr int16_t CAT_HDR       = 44;
static constexpr int16_t CAT_GAP       = 6;
static constexpr int16_t CAT_COLS      = 2;
static constexpr int16_t CAT_ROWS      = 4;
static constexpr int16_t CAT_TILE_W    = (DISPLAY_W - (CAT_COLS + 1) * CAT_GAP) / CAT_COLS;
static constexpr int16_t CAT_TILE_H    = (DISPLAY_H - CAT_HDR - (CAT_ROWS + 1) * CAT_GAP) / CAT_ROWS;

// ── Produktliste / Unterscreen-Header ────────────────────────
static constexpr int16_t SUB_HDR       = 54;
static constexpr int16_t LIST_ITEM_H   = 64;
static constexpr int16_t LIST_MAX_VIS  = (DISPLAY_H - SUB_HDR) / LIST_ITEM_H;

// ── Datumseingabe (▲ Drum Roller ▼) — Hochformat 280×456 ─────
static constexpr int16_t DRUM_COL_W    = DISPLAY_W / 3;                         // 93
static constexpr int16_t DRUM_HDR_H    = 60;
static constexpr int16_t DRUM_ARR_H    = 60;
static constexpr int16_t DRUM_ROW_H    = 60;
static constexpr int16_t DRUM_ROWS     = 3;
static constexpr int16_t DRUM_ARRUP_Y  = DRUM_HDR_H;                            // 60
static constexpr int16_t DRUM_TOP      = DRUM_ARRUP_Y + DRUM_ARR_H;             // 120
static constexpr int16_t DRUM_SEL_Y    = DRUM_TOP + DRUM_ROW_H;                 // 180
static constexpr int16_t DRUM_ARRDWN_Y = DRUM_TOP + DRUM_ROWS * DRUM_ROW_H;    // 300
static constexpr int16_t DRUM_BTN_Y    = DRUM_ARRDWN_Y + DRUM_ARR_H;           // 360

// ── Ziffernblock-Datumseingabe — Hochformat 280×456 ──────────
static constexpr int16_t NP_HDR_H  = 54;
static constexpr int16_t NP_DATE_H = 72;
static constexpr int16_t NP_TOP    = NP_HDR_H + NP_DATE_H;          // 126
static constexpr int16_t NP_COL_W  = DISPLAY_W / 3;                  // 93
static constexpr int16_t NP_ROW_H  = (DISPLAY_H - NP_TOP) / 4;      // 82

// ── Universelle Buttons ───────────────────────────────────────
static constexpr int16_t TBTN_X           = 8;
static constexpr int16_t TBTN_W           = DISPLAY_W - 16;
static constexpr int16_t TBTN_H           = 48;
static constexpr int16_t TBTN_PRIMARY_Y   = DISPLAY_H - TBTN_H - 8;
static constexpr int16_t TBTN_SECONDARY_Y = DISPLAY_H - 2 * TBTN_H - 16;

// ── IDLE-Buttons (AP-Mode) ────────────────────────────────────
static constexpr int16_t IDLE_LIST_BTN_Y = DISPLAY_H - 2 * 48 - 16;
static constexpr int16_t IDLE_INV_BTN_Y  = DISPLAY_H - 48 - 8;
static constexpr int16_t IDLE_BTN_H      = 48;

// ── Inventar-Browser ─────────────────────────────────────────
static constexpr int16_t INV_DEL_Y  = TBTN_PRIMARY_Y;
static constexpr int16_t INV_BACK_Y = TBTN_SECONDARY_Y;

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void setBrightness(uint8_t val);

    // Hauptscreen: Kategorie-Grid
    void showCategoryGrid(const std::vector<int> &catInvCounts, int warnCount, bool wifiOk);

    // Produktliste einer Kategorie
    void showProductList(const String &catName, uint16_t catColor,
                         const std::vector<CustomProduct> &products, int offset, bool wifiOk);

    void showBooting(const String &msg = "");
    void showWifiConnecting(const String &ssid, int attempt = 0);
    void showScanning();
    void showFetching(const String &barcode);
    void showDateEntry(const DateInput &date, const String &productName,
                       bool wifiOk = false, int dragCol = -1, int16_t dragPx = 0);
    void showDateEntryNumpad(const DateInput &date, const String &productName,
                             int field, const String &typed, bool wifiOk = false);
    void showPrinting();
    void showSuccess(const String &productName, const String &date, bool showReprint = true);
    void showError(const String &msg);
    void showRetrieve(const String &name, const String &storageDate,
                      const String &expiryDate, int daysLeft);
    void showInventoryItem(int index, int total, const String &name,
                           const String &expiry, int qty, int daysLeft);
    void showAPMode(const String &ssid, const String &password, const String &ip);
    void showQtyInput(const String &productName, int qty);

    // Haushalt-Auswahl: Liste aller Haushalte, myIdx = eigener Haushalt
    void showHouseholdSelect(const std::vector<String> &names, int myIdx, int selIdx);
    // Haushalt-Inventar: Artikel-Liste eines fremden Haushalts
    void showHouseholdInventory(const String &hhName, const std::vector<String> &itemNames,
                                const std::vector<int> &daysLeft, int offset, int total);

private:
    Arduino_DataBus *_bus;
    Arduino_CO5300  *_panel;
    Arduino_Canvas  *_gfx;

    void applyFont(uint8_t px);
    void textCenter(const String &s, int16_t cx, int16_t cy, uint8_t px,
                    uint16_t color, uint16_t bg = COLOR_BG);
    void textLeft(const String &s, int16_t x, int16_t y, uint8_t px,
                  uint16_t color, uint16_t bg = COLOR_BG);
    int16_t textWidth(const String &s, uint8_t px);

    void drawHeader(const String &title, uint16_t bgColor = COLOR_HEADER);
    void drawStatusBar(const String &title, uint16_t titleColor, bool showBack, bool wifiOk);
    void drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                         const String &label, uint16_t bg, uint16_t fg,
                         uint8_t textPx = 16);
    String daysLabel(int days);
};
