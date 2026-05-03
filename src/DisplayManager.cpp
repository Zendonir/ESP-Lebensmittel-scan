#include "DisplayManager.h"
#include "CategoryManager.h"
#include "FontConfig.h"
#include "fonts/gfxfont_compat.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans24pt7b.h"
#include <time.h>

static uint8_t prevPx(uint8_t px) {
    if (px >= 29) return 24;
    if (px >= 21) return 16;
    if (px >= 14) return 12;
    if (px >= 11) return  8;
    return 8;
}

static constexpr int16_t W   = DISPLAY_W;   // 280
static constexpr int16_t H   = DISPLAY_H;   // 456
static constexpr int16_t HDR = 40;   // used by legacy drawHeader()

// ── Init ──────────────────────────────────────────────────────

DisplayManager::DisplayManager() : _bus(nullptr), _panel(nullptr), _gfx(nullptr) {}

bool DisplayManager::begin() {
    Serial.printf("[Disp] CS=%d SCK=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                  LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_RST);
    _bus   = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    _panel = new Arduino_CO5300(_bus, LCD_RST, 0, W, H, 20, 0, 180, 24);
    _gfx   = new Arduino_Canvas(W, H, _panel);
    Serial.printf("[Disp] canvas ptr=%p\n", (void*)_gfx);
    bool ok = _gfx->begin();
    Serial.printf("[Disp] gfx->begin()=%d  psram free after=%u\n", ok, ESP.getFreePsram());
    if (!ok) return false;
    Serial.printf("[Disp] W=%d H=%d rotation=0\n", W, H);
    _panel->setRotation(0); // CO5300 Hardware: Portrait MADCTL
    _gfx->setRotation(0);   // Canvas: Portrait Koordinatensystem
    _gfx->fillScreen(COLOR_BG);
    _gfx->flush();
    Serial.println("[Disp] flush() done");
    _panel->setBrightness(220);
    return true;
}

void DisplayManager::setBrightness(uint8_t val) {
    if (_panel) _panel->setBrightness(val);
}

// ── Text-Helfer ───────────────────────────────────────────────

void DisplayManager::applyFont(uint8_t px) {
    if      (px >= 29) { _gfx->setFont(&FreeSans24pt7b); _gfx->setTextSize(1); }
    else if (px >= 21) { _gfx->setFont(&FreeSans18pt7b); _gfx->setTextSize(1); }
    else if (px >= 14) { _gfx->setFont(&FreeSans12pt7b); _gfx->setTextSize(1); }
    else if (px >= 11) { _gfx->setFont(&FreeSans9pt7b);  _gfx->setTextSize(1); }
    else               { _gfx->setFont(nullptr);          _gfx->setTextSize(max(1, px / 8)); }
}

int16_t DisplayManager::textWidth(const String &s, uint8_t px) {
    applyFont(px);
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(s.c_str(), 0, 100, &x1, &y1, &w, &h);
    return (int16_t)w;
}

void DisplayManager::textCenter(const String &s, int16_t cx, int16_t cy,
                                 uint8_t px, uint16_t color, uint16_t bg) {
    applyFont(px);
    _gfx->setTextColor(color, bg);
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(s.c_str(), 0, 100, &x1, &y1, &w, &h);
    _gfx->setCursor(cx - (int16_t)w / 2 - x1, cy - (int16_t)h / 2 - y1 + 100);
    _gfx->print(s);
}

void DisplayManager::textLeft(const String &s, int16_t x, int16_t y,
                               uint8_t px, uint16_t color, uint16_t bg) {
    applyFont(px);
    _gfx->setTextColor(color, bg);
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(s.c_str(), 0, 100, &x1, &y1, &w, &h);
    _gfx->setCursor(x - x1, y - y1 + 100);
    _gfx->print(s);
}

// ── Bausteine ─────────────────────────────────────────────────

void DisplayManager::drawHeader(const String &title, uint16_t bg) {
    _gfx->fillRect(0, 0, W, HDR, bg);
    textCenter(title, W / 2, HDR / 2, g_fontCfg.title, 0xFFFF, bg);
}

void DisplayManager::drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                      const String &label, uint16_t bg, uint16_t fg,
                                      uint8_t sz) {
    _gfx->fillRoundRect(x, y, w, h, g_uiCfg.btn_radius, bg);
    // Transparent text background: button is already drawn, avoid corner artifacts
    applyFont(sz);
    _gfx->setTextColor(fg);
    int16_t x1, y1; uint16_t tw, th;
    _gfx->getTextBounds(label.c_str(), 0, 100, &x1, &y1, &tw, &th);
    _gfx->setCursor(x + w / 2 - (int16_t)tw / 2 - x1,
                    y + h / 2 - (int16_t)th / 2 - y1 + 100);
    _gfx->print(label);
}

// ── Status-/Navigationsleiste ──────────────────────────────────

void DisplayManager::drawStatusBar(const String &title, uint16_t titleColor,
                                    bool showBack, bool wifiOk) {
    _gfx->fillRect(0, 0, W, SUB_HDR, COLOR_BG);

    _gfx->fillCircle(W - 8, 8, 4, wifiOk ? COLOR_OK : COLOR_DANGER);

    if (g_uiCfg.show_clock) {
        struct tm ti; char tbuf[6] = "--:--";
        if (getLocalTime(&ti, 0))
            snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        textLeft(tbuf, 6, 2, g_fontCfg.small, COLOR_SUBTEXT);
    }

    if (showBack)
        textLeft("< Zuruck", 6, SUB_HDR / 2 + 2, g_fontCfg.small, COLOR_TEXT);

    textCenter(title, W / 2, SUB_HDR / 2, g_fontCfg.title, titleColor);

    _gfx->drawFastHLine(0, SUB_HDR, W, COLOR_SURFACE);
}

String DisplayManager::daysLabel(int days) {
    if (days < 0)  return "ABGELAUFEN";
    if (days == 0) return "Heute!";
    if (days == 1) return "1 Tag";
    return String(days) + " Tage";
}

// ── Hauptscreen: Kategorie-Grid ───────────────────────────────

void DisplayManager::showCategoryGrid(const std::vector<int> &catInvCounts,
                                       int warnCount, bool wifiOk) {
    _gfx->fillScreen(COLOR_BG);

    _gfx->fillCircle(W - 8, 8, 4, wifiOk ? COLOR_OK : COLOR_DANGER);
    if (g_uiCfg.show_clock) {
        struct tm ti; char tbuf[6] = "--:--";
        if (getLocalTime(&ti, 0))
            snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        textLeft(tbuf, 6, 2, g_fontCfg.small, COLOR_SUBTEXT);
    }
    textCenter("Kategorien", W / 2, CAT_HDR / 2, g_fontCfg.title, COLOR_TEXT);
    _gfx->drawFastHLine(0, CAT_HDR, W, COLOR_SURFACE);

    int nCats = min((int)g_categories.size(), CAT_COLS * CAT_ROWS);
    for (int i = 0; i < nCats; i++) {
        int col = i % CAT_COLS, row = i / CAT_COLS;
        int16_t tx = CAT_GAP + col * (CAT_TILE_W + CAT_GAP);
        int16_t ty = CAT_HDR + CAT_GAP + row * (CAT_TILE_H + CAT_GAP);

        uint16_t bg = g_categories[i].bgColor;
        _gfx->fillRoundRect(tx, ty, CAT_TILE_W, CAT_TILE_H, g_uiCfg.btn_radius, bg);

        String name = g_categories[i].name;
        uint8_t tsz = g_fontCfg.btn;
        if (name.length() > 9 && tsz > 16) tsz = prevPx(tsz);
        if (name.length() > 20) name = name.substring(0, 20);
        textCenter(name, tx + CAT_TILE_W / 2, ty + CAT_TILE_H / 2, tsz, 0xFFFF, bg);
    }

    _gfx->flush();
}

// ── Produktliste einer Kategorie ──────────────────────────────

void DisplayManager::showProductList(const String &catName, uint16_t catColor,
                                      const std::vector<CustomProduct> &products,
                                      int offset, bool wifiOk) {
    _gfx->fillScreen(COLOR_BG);
    drawStatusBar(catName, catColor, true, wifiOk);

    if (products.empty()) {
        textCenter("Keine Produkte konfiguriert",   W / 2, SUB_HDR + 80,  g_fontCfg.small, COLOR_SUBTEXT);
        textCenter("Web-Interface: Vorlagen > Neu", W / 2, SUB_HDR + 100, g_fontCfg.small, COLOR_SUBTEXT);
        _gfx->flush();
        return;
    }

    int visible = min((int)LIST_MAX_VIS, (int)products.size() - offset);
    for (int i = 0; i < visible; i++) {
        const auto &p = products[offset + i];
        int16_t iy = SUB_HDR + i * LIST_ITEM_H;

        _gfx->fillRoundRect(4, iy + 2, W - 8, LIST_ITEM_H - 4, LIST_RADIUS, COLOR_SURFACE);

        int16_t textY = iy + LIST_ITEM_H / 2 - 8;
        String name = p.name.length() > 26 ? p.name.substring(0, 26) : p.name;
        textLeft(name, 14, textY, g_fontCfg.body, COLOR_TEXT, COLOR_SURFACE);

        if (p.defaultDays > 0) {
            String d = String(p.defaultDays) + "d";
            textLeft(d, W - 38 - textWidth(d, g_fontCfg.small), textY + 4,
                     g_fontCfg.small, COLOR_ACCENT, COLOR_SURFACE);
        }

        textLeft(">", W - 20, textY, g_fontCfg.body, COLOR_SUBTEXT, COLOR_SURFACE);
    }

    if ((int)products.size() > LIST_MAX_VIS) {
        int total = products.size();
        int barH  = LIST_MAX_VIS * LIST_ITEM_H;
        int markH = max(8, barH * LIST_MAX_VIS / total);
        int markY = SUB_HDR + (barH - markH) * offset / max(1, total - LIST_MAX_VIS);
        _gfx->fillRect(W - 4, SUB_HDR, 4, barH, COLOR_SURFACE);
        _gfx->fillRect(W - 4, markY, 4, markH, catColor);
    }

    _gfx->flush();
}

// ── Booting / WiFi ────────────────────────────────────────────

void DisplayManager::showBooting(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    textCenter("Lebensmittel", W / 2, H / 2 - 22, g_fontCfg.title, COLOR_ACCENT);
    textCenter("Scanner",      W / 2, H / 2 +  4, g_fontCfg.title, COLOR_ACCENT);
    _gfx->drawFastHLine(80, H / 2 + 22, W - 160, COLOR_SURFACE);
    if (!msg.isEmpty())
        textCenter(msg, W / 2, H / 2 + 42, g_fontCfg.small, COLOR_SUBTEXT);
    _gfx->flush();
}

void DisplayManager::showWifiConnecting(const String &ssid, int attempt) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WLAN Verbindung");
    textCenter("Verbinde mit:", W / 2, H/2 - 80, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter(ssid,            W / 2, H/2 - 44, g_fontCfg.body,  COLOR_ACCENT);
    int barW   = W - 80;
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(40, H/2 - 10, barW, 10, COLOR_SURFACE);
    if (filled > 0) _gfx->fillRect(40, H/2 - 10, filled, 10, COLOR_ACCENT);
    textCenter("Bitte warten...", W / 2, H/2 + 22, g_fontCfg.small, COLOR_SUBTEXT);
    _gfx->flush();
}

// ── Scanning / Fetching ───────────────────────────────────────

void DisplayManager::showScanning() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");
    int bx = 20, by = 60, bw = W - 40, bh = 140;
    for (int x = 0; x < bw; x += 7)
        _gfx->fillRect(bx + x, by, (x % 14 < 7) ? 4 : 2, bh, COLOR_TEXT);
    _gfx->fillRect(bx, by + bh + 4, bw, 2, COLOR_SUBTEXT);
    textCenter("GM861 vor den Scanner halten",  W / 2, by + bh + 28, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter("EAN13  EAN8  QR  DataMatrix",   W / 2, by + bh + 48, g_fontCfg.small, COLOR_SUBTEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Abbrechen", COLOR_SURFACE, COLOR_TEXT, g_fontCfg.btn);
    _gfx->flush();
}

void DisplayManager::showFetching(const String &barcode) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Suche Produkt...");
    textCenter("Open Food Facts", W / 2, H/2 - 80, g_fontCfg.small, COLOR_SUBTEXT);
    String b = barcode.length() > 24 ? barcode.substring(0, 24) : barcode;
    textCenter(b, W / 2, H/2 - 40, g_fontCfg.body, COLOR_ACCENT);
    static uint8_t dots = 0;
    String d = ""; for (int i = 0; i < (dots % 4); i++) d += " .";
    _gfx->fillRect(0, H/2 - 10, W, 30, COLOR_BG);
    textCenter(d, W / 2, H/2 + 4, g_fontCfg.body, COLOR_TEXT); dots++;
    _gfx->flush();
}

// ── Datumseingabe (Drum Roller) ───────────────────────────────

void DisplayManager::showDateEntry(const DateInput &d, const String &productName,
                                    bool wifiOk, int dragCol, int16_t dragPx) {
    _gfx->fillScreen(COLOR_BG);

    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn, W / 2, DRUM_HDR_H / 2, g_fontCfg.body, COLOR_TEXT);

    _gfx->drawFastVLine(    DRUM_COL_W, DRUM_HDR_H, DRUM_BTN_Y - DRUM_HDR_H, COLOR_SURFACE);
    _gfx->drawFastVLine(2 * DRUM_COL_W, DRUM_HDR_H, DRUM_BTN_Y - DRUM_HDR_H, COLOR_SURFACE);

    for (int c = 0; c < 3; c++) {
        int16_t bx = c * DRUM_COL_W + 6;
        int16_t bw = DRUM_COL_W - 12;
        int16_t by = DRUM_ARRUP_Y + 3;
        int16_t bh = DRUM_ARR_H - 6;
        _gfx->fillRoundRect(bx, by, bw, bh, 8, COLOR_SURFACE);
        _gfx->drawRoundRect(bx, by, bw, bh, 8, COLOR_ACCENT);
        int16_t cx = bx + bw / 2, cy = by + bh / 2;
        _gfx->fillTriangle(cx, cy - 10, cx - 13, cy + 8, cx + 13, cy + 8, COLOR_ACCENT);
    }

    int  baseVals[3] = {d.day, d.month, d.year};
    int  minVals[3]  = {1, 1, 2024};
    int  maxVals[3]  = {31, 12, 2099};
    int16_t centerY  = DRUM_SEL_Y + DRUM_ROW_H / 2;

    for (int c = 0; c < 3; c++) {
        int16_t colCX = c * DRUM_COL_W + DRUM_COL_W / 2;
        int16_t px    = (c == dragCol) ? dragPx : 0;
        int halfRange = (px != 0) ? 3 : 1;

        for (int r = -halfRange; r <= halfRange; r++) {
            int val = baseVals[c] + r;
            if (val < minVals[c] || val > maxVals[c]) continue;

            int16_t rowCY = DRUM_SEL_Y + r * DRUM_ROW_H + DRUM_ROW_H / 2 + px;

            int dist10 = abs(rowCY - centerY) * 10 / DRUM_ROW_H;
            uint16_t clr; uint8_t tpx;
            if      (dist10 <  5) { tpx = g_fontCfg.value;                   clr = COLOR_TEXT; }
            else if (dist10 < 15) { tpx = prevPx(g_fontCfg.value);           clr = COLOR_SUBTEXT; }
            else                  { tpx = prevPx(prevPx(g_fontCfg.value));   clr = 0x2104; }

            if (rowCY < DRUM_TOP + tpx / 2 || rowCY > DRUM_ARRDWN_Y - tpx / 2) continue;

            String valStr = (c == 2) ? String(val)
                                     : (val < 10 ? "0" + String(val) : String(val));
            textCenter(valStr, colCX, rowCY, tpx, clr);
        }
    }

    _gfx->drawRoundRect(2, DRUM_SEL_Y, W - 4, DRUM_ROW_H, 6, COLOR_ACCENT);

    for (int c = 0; c < 3; c++) {
        int16_t bx = c * DRUM_COL_W + 6;
        int16_t bw = DRUM_COL_W - 12;
        int16_t by = DRUM_ARRDWN_Y + 3;
        int16_t bh = DRUM_ARR_H - 6;
        _gfx->fillRoundRect(bx, by, bw, bh, 8, COLOR_SURFACE);
        _gfx->drawRoundRect(bx, by, bw, bh, 8, COLOR_ACCENT);
        int16_t cx = bx + bw / 2, cy = by + bh / 2;
        _gfx->fillTriangle(cx, cy + 10, cx - 13, cy - 8, cx + 13, cy - 8, COLOR_ACCENT);
    }

    int16_t btnY = DRUM_BTN_Y + 4;
    int16_t btnH = H - DRUM_BTN_Y - 8;
    int16_t halfW = W / 2 - 6;
    _gfx->drawRoundRect(4, btnY, halfW, btnH, 8, COLOR_DANGER);
    textCenter("< Zuruck", 4 + halfW / 2, btnY + btnH / 2, g_fontCfg.small, COLOR_DANGER);
    _gfx->drawRoundRect(W / 2 + 2, btnY, halfW, btnH, 8, COLOR_OK);
    textCenter("OK", W / 2 + 2 + halfW / 2, btnY + btnH / 2, g_fontCfg.btn, COLOR_OK);

    _gfx->flush();
}

// ── Drucken ───────────────────────────────────────────────────

void DisplayManager::showPrinting() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Drucke Etikett...", COLOR_ACCENT);
    int16_t px = W / 2 - 36, py = H / 2 - 60;
    _gfx->fillRoundRect(px, py,      72, 50, 6, COLOR_SURFACE);
    _gfx->fillRect     (px + 8, py - 20, 56, 26, COLOR_SUBTEXT);
    _gfx->fillRect     (px + 14, py + 26, 44, 32, 0xFFFF);
    for (int l = 0; l < 3; l++)
        _gfx->fillRect(px + 18, py + 30 + l * 9, 36, 4, COLOR_SURFACE);
    textCenter("Bitte warten...", W / 2, H / 2 + 20, g_fontCfg.small, COLOR_SUBTEXT);
    _gfx->flush();
}

// ── Erfolg / Fehler ───────────────────────────────────────────

void DisplayManager::showSuccess(const String &productName, const String &date, bool showReprint) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Eingelagert!", COLOR_BTN_OK);
    int16_t cy = H / 2 - 60;
    _gfx->fillCircle(W / 2, cy, 50, COLOR_OK);
    for (int t = 0; t < 5; t++) {
        _gfx->drawLine(W/2-26+t, cy,     W/2-4+t,  cy+24, COLOR_BG);
        _gfx->drawLine(W/2-4+t,  cy+24,  W/2+26+t, cy-18, COLOR_BG);
    }
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn,           W / 2, H/2 + 20, g_fontCfg.body,  COLOR_TEXT);
    textCenter("MHD: "+date, W / 2, H/2 + 50, g_fontCfg.small, COLOR_SUBTEXT);
    if (showReprint) {
        uint8_t splitSz = min(g_fontCfg.btn, (uint8_t)24);
        drawTouchButton(TBTN_X,                      TBTN_SECONDARY_Y, (TBTN_W - 4) / 2, TBTN_H,
                        "Nochmal", COLOR_BTN_BACK, COLOR_TEXT, splitSz);
        drawTouchButton(TBTN_X + (TBTN_W + 4) / 2, TBTN_SECONDARY_Y, (TBTN_W - 4) / 2, TBTN_H,
                        "Weiter",  COLOR_BTN_OK,   COLOR_TEXT, splitSz);
    }
    _gfx->flush();
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);
    int16_t cy = H / 2 - 60;
    _gfx->fillCircle(W / 2, cy, 50, COLOR_DANGER);
    textCenter("!", W / 2, cy - 12, 32, COLOR_BG, COLOR_DANGER);
    textCenter(msg, W / 2, H/2 + 20, g_fontCfg.small, COLOR_TEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H, "OK", COLOR_BTN_BACK, COLOR_TEXT, g_fontCfg.btn);
    _gfx->flush();
}

// ── Auslagerung (Label-Barcode gescannt) ──────────────────────

void DisplayManager::showRetrieve(const String &name, const String &storageDate,
                                   const String &expiryDate, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Auslagerung", COLOR_ACCENT);
    String n = name.length() > 28 ? name.substring(0, 28) : name;
    textCenter(n, W / 2, H/2 - 80, g_fontCfg.body,  COLOR_TEXT);
    textCenter("Eingelagert: " + storageDate, W / 2, H/2 - 42, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter("MHD: " + expiryDate,          W / 2, H/2 - 20, g_fontCfg.small, COLOR_SUBTEXT);
    uint16_t sc = (daysLeft < 0)             ? COLOR_DANGER
                : (daysLeft <= DANGER_DAYS)  ? COLOR_DANGER
                : (daysLeft <= WARNING_DAYS) ? COLOR_WARN
                :                              COLOR_OK;
    textCenter(daysLabel(daysLeft), W / 2, H/2 + 20, g_fontCfg.value, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W / 2, H/2 + 60, g_fontCfg.small, COLOR_SUBTEXT);
    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, TBTN_H,
                    "Behalten", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Auslagern", COLOR_BTN_OK, COLOR_TEXT, g_fontCfg.btn);
    _gfx->flush();
}

// ── Inventar-Browser ──────────────────────────────────────────

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);
    uint16_t sc = (daysLeft < 0 || daysLeft <= DANGER_DAYS) ? COLOR_DANGER
                : (daysLeft <= WARNING_DAYS)                  ? COLOR_WARN
                :                                               COLOR_OK;
    _gfx->fillRect(0, 0, W, 6, sc);
    _gfx->fillRect(0, 6, W, HDR - 6, COLOR_HEADER);
    textCenter(String(index + 1) + " / " + String(total), W / 2, HDR / 2 + 3, g_fontCfg.body, 0xFFFF, COLOR_HEADER);

    String n = name.length() > 22 ? name.substring(0, 22) : name;
    textCenter(n, W / 2, HDR + 28, g_fontCfg.body, COLOR_TEXT);
    if (name.length() > 22)
        textCenter(name.substring(22, 44), W / 2, HDR + 54, g_fontCfg.body, COLOR_TEXT);

    textCenter("MHD:   " + expiry,                W / 2, H/2 - 40, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter("Menge: " + String(qty) + " Stk.", W / 2, H/2 - 18, g_fontCfg.small, COLOR_SUBTEXT);

    textCenter(daysLabel(daysLeft), W / 2, H/2 + 30, g_fontCfg.value, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W / 2, H/2 + 68, g_fontCfg.small, COLOR_SUBTEXT);

    textCenter("< wischen >", W / 2, TBTN_SECONDARY_Y - 20, g_fontCfg.small, COLOR_SURFACE);
    drawTouchButton(TBTN_X, INV_BACK_Y, TBTN_W, TBTN_H,
                    "Zurueck",          COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    drawTouchButton(TBTN_X, INV_DEL_Y,  TBTN_W, TBTN_H,
                    "Artikel loeschen", COLOR_DANGER,  COLOR_TEXT,    g_fontCfg.btn);
    _gfx->flush();
}

// ── Ziffernblock-Datumseingabe ────────────────────────────────

void DisplayManager::showDateEntryNumpad(const DateInput &d, const String &productName,
                                          int field, const String &typed, bool wifiOk) {
    _gfx->fillScreen(COLOR_BG);

    _gfx->fillRect(0, 0, W, NP_HDR_H, COLOR_HEADER);
    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn, W / 2, NP_HDR_H / 2, g_fontCfg.body, COLOR_TEXT, COLOR_HEADER);
    _gfx->fillCircle(W - 8, 8, 4, wifiOk ? COLOR_OK : COLOR_DANGER);

    static const char* fldLabel[] = {"Tag", "Monat", "Jahr"};
    int fldVals[3] = {d.day, d.month, d.year % 100};

    for (int i = 0; i < 3; i++) {
        int16_t bx = i * NP_COL_W;
        int16_t by = NP_HDR_H;
        bool    active = (i == field);
        uint16_t bg    = active ? COLOR_ACCENT  : COLOR_SURFACE;
        uint16_t fg    = active ? 0x0000        : COLOR_TEXT;
        uint16_t lclr  = active ? 0x0000        : COLOR_SUBTEXT;

        _gfx->fillRect(bx, by, NP_COL_W, NP_DATE_H, bg);
        textCenter(fldLabel[i], bx + NP_COL_W / 2, by + 14, g_fontCfg.small, lclr, bg);

        String valStr;
        if      (i < field)  valStr = (fldVals[i] < 10 ? "0" : "") + String(fldVals[i]);
        else if (i == field) valStr = typed.isEmpty() ? "__" : (typed.length()==1 ? typed+"_" : typed);
        else                 valStr = "--";

        textCenter(valStr, bx + NP_COL_W / 2, by + NP_DATE_H / 2 + 8, g_fontCfg.value, fg, bg);
    }
    _gfx->drawFastVLine(    NP_COL_W, NP_HDR_H, NP_DATE_H, COLOR_BG);
    _gfx->drawFastVLine(2 * NP_COL_W, NP_HDR_H, NP_DATE_H, COLOR_BG);

    const char* lbl[4][3] = {
        {"1","2","3"}, {"4","5","6"}, {"7","8","9"},
        {"<","0", field == 2 ? "OK" : ">"}
    };
    uint16_t bbg[4][3] = {
        {COLOR_SURFACE,  COLOR_SURFACE, COLOR_SURFACE},
        {COLOR_SURFACE,  COLOR_SURFACE, COLOR_SURFACE},
        {COLOR_SURFACE,  COLOR_SURFACE, COLOR_SURFACE},
        {COLOR_BTN_BACK, COLOR_SURFACE, field == 2 ? COLOR_BTN_OK : COLOR_ACCENT}
    };

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            int16_t bx = col * NP_COL_W + 4;
            int16_t by = NP_TOP + row * NP_ROW_H + 4;
            int16_t bw = NP_COL_W - 8;
            int16_t bh = NP_ROW_H - 8;
            _gfx->fillRoundRect(bx, by, bw, bh, 10, bbg[row][col]);
            uint8_t sz = (row == 3 && col != 1) ? g_fontCfg.btn : g_fontCfg.key;
            textCenter(lbl[row][col], bx + bw / 2, by + bh / 2, sz, COLOR_TEXT, bbg[row][col]);
        }
    }

    _gfx->flush();
}

// ── AP-Modus ──────────────────────────────────────────────────

void DisplayManager::showAPMode(const String &ssid, const String &password, const String &ip) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WiFi Einrichtung", COLOR_WARN);
    textCenter("Kein WLAN konfiguriert!",    W / 2,  80, g_fontCfg.small, COLOR_WARN);
    textCenter("Netz:",                      W / 2, 120, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter(ssid,                         W / 2, 150, g_fontCfg.body,  COLOR_ACCENT);
    textCenter("PW:",                        W / 2, 192, g_fontCfg.small, COLOR_SUBTEXT);
    textCenter(password,                     W / 2, 222, g_fontCfg.body,  COLOR_TEXT);
    _gfx->drawFastHLine(20, 258, W - 40, COLOR_SURFACE);
    textCenter("Browser: http://" + ip,      W / 2, 280, g_fontCfg.small, COLOR_ACCENT);
    drawTouchButton(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H,
                    "Zur Produktliste",  COLOR_BTN_OK,   COLOR_TEXT, g_fontCfg.btn);
    drawTouchButton(TBTN_X, IDLE_INV_BTN_Y,  TBTN_W, IDLE_BTN_H,
                    "Inventar anzeigen", COLOR_BTN_BACK, COLOR_TEXT, g_fontCfg.btn);
    _gfx->flush();
}

// ── Anzahl / Portionen-Eingabe ────────────────────────────────

void DisplayManager::showQtyInput(const String &productName, int qty) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Anzahl / Portionen");

    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn, W / 2, 75, g_fontCfg.body, COLOR_TEXT);

    // Minus-Taste (links)
    _gfx->fillRoundRect(8, 145, 90, 90, 14, COLOR_SURFACE);
    textCenter("-", 53, 190, g_fontCfg.value, COLOR_TEXT, COLOR_SURFACE);

    // Anzahl (groß, mittig)
    textCenter(String(qty), W / 2, 190, g_fontCfg.value, COLOR_ACCENT);

    // Plus-Taste (rechts)
    _gfx->fillRoundRect(182, 145, 90, 90, 14, COLOR_SURFACE);
    textCenter("+", 227, 190, g_fontCfg.value, COLOR_TEXT, COLOR_SURFACE);

    // Hinweis
    textCenter("Portionen / Stueck", W / 2, 260, g_fontCfg.small, COLOR_SUBTEXT);

    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, TBTN_H,
                    "< Zurueck", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "OK", COLOR_BTN_OK, COLOR_TEXT, g_fontCfg.btn);
    _gfx->flush();
}

// ── Haushalt-Auswahl ─────────────────────────────────────────

void DisplayManager::showHouseholdSelect(const std::vector<String> &names, int myIdx, int selIdx) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Haushalte");

    int n = (int)names.size();
    if (n == 0) {
        textCenter("Kein Server konfiguriert", W / 2, H / 2, g_fontCfg.body, COLOR_SUBTEXT);
        drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                        "< Zurueck", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
        _gfx->flush();
        return;
    }

    int startY = 54;
    int rowH   = min(60, (TBTN_SECONDARY_Y - startY) / max(1, n));

    for (int i = 0; i < n && i < 6; i++) {
        int16_t y   = startY + i * rowH;
        bool isMine = (i == myIdx);
        bool isSel  = (i == selIdx);
        uint16_t bg = isSel ? COLOR_ACCENT : (isMine ? COLOR_SURFACE : COLOR_BG);
        uint16_t fg = isSel ? COLOR_BG     : (isMine ? COLOR_TEXT    : COLOR_SUBTEXT);
        _gfx->fillRoundRect(8, y + 4, W - 16, rowH - 8, 10, bg);
        String label = names[i];
        if (isMine) label += " (dieses Geraet)";
        textCenter(label, W / 2, y + rowH / 2, g_fontCfg.body, fg, bg);
    }

    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, TBTN_H,
                    "Oeffnen", COLOR_BTN_OK, COLOR_TEXT, g_fontCfg.btn);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "< Zurueck", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    _gfx->flush();
}

// ── Haushalt-Inventar (read-only) ────────────────────────────

void DisplayManager::showHouseholdInventory(const String &hhName,
                                             const std::vector<String> &itemNames,
                                             const std::vector<int> &daysLeft,
                                             int offset, int total) {
    _gfx->fillScreen(COLOR_BG);
    _gfx->fillRect(0, 0, W, SUB_HDR, COLOR_HEADER);
    textCenter(hhName, W / 2, SUB_HDR / 2, g_fontCfg.header, COLOR_TEXT, COLOR_HEADER);

    int vis = min((int)itemNames.size(), LIST_MAX_VIS);
    for (int i = 0; i < vis; i++) {
        int16_t ry  = SUB_HDR + i * LIST_ITEM_H;
        bool even   = (i % 2 == 0);
        uint16_t bg = even ? COLOR_SURFACE : COLOR_BG;
        _gfx->fillRect(0, ry, W, LIST_ITEM_H, bg);

        int dl = daysLeft[i];
        uint16_t clr;
        if      (dl < 0)  clr = 0xF800;
        else if (dl < 3)  clr = 0xFD20;
        else              clr = COLOR_TEXT;

        textLeft(itemNames[i], 10, ry + LIST_ITEM_H / 2, g_fontCfg.body, clr, bg);
        String dlStr = (dl < 0) ? "abg." : (dl == 0 ? "heute" : String(dl) + "d");
        textCenter(dlStr, W - 30, ry + LIST_ITEM_H / 2, g_fontCfg.small, clr, bg);
    }

    if (total > LIST_MAX_VIS) {
        String pg = String(offset / LIST_MAX_VIS + 1) + "/" +
                    String((total + LIST_MAX_VIS - 1) / LIST_MAX_VIS);
        textCenter(pg, W / 2, TBTN_SECONDARY_Y - 16, g_fontCfg.small, COLOR_SUBTEXT);
    }

    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, (TBTN_W - 8) / 2, TBTN_H,
                    "< Vor", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    drawTouchButton(TBTN_X + (TBTN_W + 8) / 2, TBTN_SECONDARY_Y, (TBTN_W - 8) / 2, TBTN_H,
                    "Vor >", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "< Zurueck", COLOR_SURFACE, COLOR_SUBTEXT, g_fontCfg.small);
    _gfx->flush();
}
