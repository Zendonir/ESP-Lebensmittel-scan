#include "DisplayManager.h"
#include "CategoryManager.h"
#include <time.h>

static constexpr int16_t W   = DISPLAY_W;   // 456
static constexpr int16_t H   = DISPLAY_H;   // 280
static constexpr int16_t HDR = 40;   // used by legacy drawHeader()

// ── Init ──────────────────────────────────────────────────────

DisplayManager::DisplayManager() : _bus(nullptr), _panel(nullptr), _gfx(nullptr) {}

bool DisplayManager::begin() {
    Serial.printf("[Disp] CS=%d SCK=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                  LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_RST);
    _bus   = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    _panel = new Arduino_CO5300(_bus, LCD_RST, 0, 280, 456, 20, 0, 180, 24);
    _gfx   = new Arduino_Canvas(280, 456, _panel);
    Serial.printf("[Disp] canvas ptr=%p\n", (void*)_gfx);
    bool ok = _gfx->begin();
    Serial.printf("[Disp] gfx->begin()=%d  psram free after=%u\n", ok, ESP.getFreePsram());
    if (!ok) return false;
    _gfx->setRotation(1);
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

int16_t DisplayManager::textWidth(const String &s, uint8_t sz) {
    return s.length() * 6 * sz;
}
void DisplayManager::textCenter(const String &s, int16_t cx, int16_t cy,
                                 uint8_t sz, uint16_t color, uint16_t bg) {
    _gfx->setTextSize(sz);
    _gfx->setTextColor(color, bg);
    _gfx->setCursor(cx - textWidth(s, sz) / 2, cy - 4 * sz);
    _gfx->print(s);
}
void DisplayManager::textLeft(const String &s, int16_t x, int16_t y,
                               uint8_t sz, uint16_t color, uint16_t bg) {
    _gfx->setTextSize(sz);
    _gfx->setTextColor(color, bg);
    _gfx->setCursor(x, y);
    _gfx->print(s);
}

// ── Bausteine ─────────────────────────────────────────────────

void DisplayManager::drawHeader(const String &title, uint16_t bg) {
    _gfx->fillRect(0, 0, W, HDR, bg);
    textCenter(title, W / 2, HDR / 2, 2, 0xFFFF, bg);
}

void DisplayManager::drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                      const String &label, uint16_t bg, uint16_t fg,
                                      uint8_t sz) {
    _gfx->fillRoundRect(x, y, w, h, 10, bg);
    textCenter(label, x + w / 2, y + h / 2, sz, fg, bg);
}

// ── Status-/Navigationsleiste ──────────────────────────────────
// showBack=true: zeigt "< Zuruck" links, wifiOk: WiFi-Indikator rechts

void DisplayManager::drawStatusBar(const String &title, uint16_t titleColor,
                                    bool showBack, bool wifiOk) {
    _gfx->fillRect(0, 0, W, SUB_HDR, COLOR_BG);

    // WiFi-Indikator oben rechts
    uint16_t wc = wifiOk ? COLOR_OK : COLOR_DANGER;
    _gfx->fillCircle(W - 8, 8, 4, wc);

    // Uhrzeit oben links
    struct tm ti; char tbuf[6] = "--:--";
    if (getLocalTime(&ti, 0))
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    textLeft(tbuf, 6, 2, 1, COLOR_SUBTEXT);

    // Zurück-Button (zweite Zeile links) oder nur Zeit zeigen
    if (showBack)
        textLeft("< Zuruck", 6, SUB_HDR / 2 + 2, 1, COLOR_TEXT);

    // Titel mittig
    textCenter(title, W / 2, SUB_HDR / 2, 2, titleColor);

    _gfx->drawFastHLine(0, SUB_HDR, W, COLOR_SURFACE);
}

String DisplayManager::daysLabel(int days) {
    if (days < 0)  return "ABGELAUFEN";
    if (days == 0) return "Heute!";
    if (days == 1) return "1 Tag";
    return String(days) + " Tage";
}

// ── Kategorie-Icons ───────────────────────────────────────────

void DisplayManager::drawCategoryIcon(uint8_t cat, int16_t cx, int16_t cy, uint8_t s) {
    switch (cat) {
    case 0: // Fleisch / Steak
        _gfx->fillRoundRect(cx-19*s,cy-9*s,38*s,20*s,6*s,0xD00A);
        _gfx->fillRect(cx+13*s,cy-18*s,5*s,36*s,0xEF5B);
        _gfx->fillCircle(cx+15*s,cy-20*s,6*s,0xEF5B);
        _gfx->fillCircle(cx+15*s,cy+20*s,6*s,0xEF5B);
        break;
    case 1: // Geflügel
        _gfx->fillCircle(cx-2*s,cy-9*s,17*s,0xD580);
        _gfx->fillRoundRect(cx-4*s,cy+8*s,8*s,18*s,4*s,0xEF7B);
        _gfx->fillCircle(cx,cy+27*s,6*s,0xD580);
        break;
    case 2: // Fisch
        _gfx->fillEllipse(cx-4*s,cy,20*s,12*s,0x07DF);
        _gfx->fillTriangle(cx+15*s,cy-12*s,cx+15*s,cy+12*s,cx+27*s,cy,0x07DF);
        _gfx->fillCircle(cx-13*s,cy-4*s,3*s,0xFFFF);
        _gfx->fillTriangle(cx-6*s,cy-12*s,cx+4*s,cy-12*s,cx-1*s,cy-19*s,0x07BF);
        break;
    case 3: // Gemüse
        _gfx->fillCircle(cx-8*s,cy-6*s,11*s,0x07E0);
        _gfx->fillCircle(cx+8*s,cy-6*s,11*s,0x07E0);
        _gfx->fillCircle(cx,cy-15*s,11*s,0x4FE0);
        _gfx->fillRect(cx-3*s,cy+6*s,6*s,14*s,0x5B00);
        break;
    case 4: // Backwaren
        _gfx->fillCircle(cx-7*s,cy+5*s,13*s,0xF800);
        _gfx->fillCircle(cx+9*s,cy+1*s,11*s,0xFD40);
        _gfx->fillRect(cx-5*s,cy-17*s,2*s,8*s,0x0460);
        _gfx->fillEllipse(cx-1*s,cy-16*s,5*s,3*s,0x04A0);
        break;
    case 5: // Sonstiges
        _gfx->drawCircle(cx,cy,19*s,0xFFFF);
        _gfx->drawCircle(cx,cy,13*s,0xFFFF);
        _gfx->fillRect(cx-11*s,cy-16*s,2*s,30*s,0xFFFF);
        _gfx->fillRect(cx-13*s,cy-16*s,2*s,10*s,0xFFFF);
        _gfx->fillRect(cx-9*s,cy-16*s,2*s,10*s,0xFFFF);
        _gfx->fillRect(cx+9*s,cy-16*s,2*s,30*s,0xFFFF);
        _gfx->fillRoundRect(cx+9*s,cy-16*s,5*s,14*s,2*s,0xD6DB);
        break;
    case 6:
        _gfx->fillRoundRect(cx-19*s,cy-2*s,38*s,16*s,6*s,0x8320);
        _gfx->fillCircle(cx-8*s,cy-5*s,12*s,0xA400);
        _gfx->fillCircle(cx+8*s,cy-5*s,12*s,0xA400);
        _gfx->drawLine(cx,cy-15*s,cx,cy+10*s,0x6240);
        break;
    case 7: {
        uint16_t sc=0xB7FF;
        for(int t=0;t<=s;t++){
            _gfx->drawLine(cx+t,cy-21*s,cx+t,cy+21*s,sc);
            _gfx->drawLine(cx-18*s+t,cy-11*s,cx+18*s+t,cy+11*s,sc);
            _gfx->drawLine(cx-18*s+t,cy+11*s,cx+18*s+t,cy-11*s,sc);
        }
        _gfx->drawLine(cx-6*s,cy-15*s,cx,cy-21*s,sc);
        _gfx->drawLine(cx+6*s,cy-15*s,cx,cy-21*s,sc);
        _gfx->drawLine(cx-6*s,cy+15*s,cx,cy+21*s,sc);
        _gfx->drawLine(cx+6*s,cy+15*s,cx,cy+21*s,sc);
        _gfx->fillCircle(cx,cy,4*s,sc);
        break;
    }
    }
}

// ── Hauptscreen: Kategorie-Grid ───────────────────────────────

void DisplayManager::showCategoryGrid(const std::vector<int> &catInvCounts,
                                       int warnCount, bool wifiOk) {
    _gfx->fillScreen(COLOR_BG);

    // Status-Leiste
    uint16_t wc = wifiOk ? COLOR_OK : COLOR_DANGER;
    _gfx->fillCircle(W - 8, 8, 4, wc);
    struct tm ti; char tbuf[6] = "--:--";
    if (getLocalTime(&ti, 0))
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    textLeft(tbuf, 6, 2, 1, COLOR_SUBTEXT);
    textCenter("Kategorien", W / 2, CAT_HDR / 2, 2, COLOR_TEXT);
    _gfx->drawFastHLine(0, CAT_HDR, W, COLOR_SURFACE);

    int nCats = min((int)g_categories.size(), CAT_COLS * CAT_ROWS);
    for (int i = 0; i < nCats; i++) {
        int col = i % CAT_COLS, row = i / CAT_COLS;
        int16_t tx = CAT_GAP + col * (CAT_TILE_W + CAT_GAP);
        int16_t ty = CAT_HDR + CAT_GAP + row * (CAT_TILE_H + CAT_GAP);

        uint16_t bg = g_categories[i].bgColor;
        _gfx->fillRoundRect(tx, ty, CAT_TILE_W, CAT_TILE_H, 12, bg);

        // Inventar-Badge oben rechts (wenn Einträge vorhanden)
        int cnt = (i < (int)catInvCounts.size()) ? catInvCounts[i] : 0;
        if (cnt > 0) {
            uint16_t badgeBg = (warnCount > 0) ? COLOR_WARN : COLOR_ACCENT;
            _gfx->fillCircle(tx + CAT_TILE_W - 12, ty + 12, 11, badgeBg);
            String cs = cnt > 99 ? "9+" : String(cnt);
            textCenter(cs, tx + CAT_TILE_W - 12, ty + 8, 1, COLOR_BG, badgeBg);
        }

        // Icon zentriert, etwas über Mitte (Platz für Text unten)
        drawCategoryIcon(i, tx + CAT_TILE_W / 2, ty + CAT_TILE_H / 2 - 10, 2);

        // Kategorie-Name unten
        String name = g_categories[i].name;
        if (name.length() > 10) name = name.substring(0, 10);
        textCenter(name, tx + CAT_TILE_W / 2, ty + CAT_TILE_H - 14, 2, 0xFFFF, bg);
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
        textCenter("Keine Produkte konfiguriert",  W / 2, SUB_HDR + 80, 1, COLOR_SUBTEXT);
        textCenter("Web-Interface: Vorlagen > Neu", W / 2, SUB_HDR + 100, 1, COLOR_SUBTEXT);
        _gfx->flush();
        return;
    }

    int visible = min((int)LIST_MAX_VIS, (int)products.size() - offset);
    for (int i = 0; i < visible; i++) {
        const auto &p = products[offset + i];
        int16_t iy = SUB_HDR + i * LIST_ITEM_H;

        _gfx->fillRoundRect(4, iy + 2, W - 8, LIST_ITEM_H - 4, 6, COLOR_SURFACE);

        String name = p.name.length() > 26 ? p.name.substring(0, 26) : p.name;
        textLeft(name, 14, iy + 10, 2, COLOR_TEXT, COLOR_SURFACE);

        if (p.defaultDays > 0) {
            String d = String(p.defaultDays) + "d";
            textLeft(d, W - 38 - textWidth(d, 1), iy + 16, 1, COLOR_ACCENT, COLOR_SURFACE);
        }

        // Pfeil rechts
        textLeft(">", W - 20, iy + 10, 2, COLOR_SUBTEXT, COLOR_SURFACE);
    }

    // Scrollbalken
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
    textCenter("Lebensmittel", W / 2, H / 2 - 22, 2, COLOR_ACCENT);
    textCenter("Scanner",      W / 2, H / 2 +  4, 2, COLOR_ACCENT);
    _gfx->drawFastHLine(80, H / 2 + 22, W - 160, COLOR_SURFACE);
    if (!msg.isEmpty())
        textCenter(msg, W / 2, H / 2 + 42, 1, COLOR_SUBTEXT);
    _gfx->flush();
}

void DisplayManager::showWifiConnecting(const String &ssid, int attempt) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WLAN Verbindung");
    textCenter("Verbinde mit:", W / 2,  76, 1, COLOR_SUBTEXT);
    textCenter(ssid,            W / 2, 108, 2, COLOR_ACCENT);
    int barW   = W - 80;
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(40, 136, barW, 10, COLOR_SURFACE);
    if (filled > 0) _gfx->fillRect(40, 136, filled, 10, COLOR_ACCENT);
    textCenter("Bitte warten...", W / 2, 164, 1, COLOR_SUBTEXT);
    _gfx->flush();
}

// ── Scanning / Fetching ───────────────────────────────────────

void DisplayManager::showScanning() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");
    int bx = 20, by = 48, bw = W - 40, bh = 106;
    for (int x = 0; x < bw; x += 7)
        _gfx->fillRect(bx + x, by, (x % 14 < 7) ? 4 : 2, bh, COLOR_TEXT);
    _gfx->fillRect(bx, by + bh + 4, bw, 2, COLOR_SUBTEXT);
    textCenter("GM861 vor den Scanner halten",   W / 2, 172, 1, COLOR_SUBTEXT);
    textCenter("EAN13  EAN8  QR  DataMatrix", W / 2, 192, 1, COLOR_SUBTEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H, "Abbrechen", COLOR_SURFACE, COLOR_TEXT);
    _gfx->flush();
}

void DisplayManager::showFetching(const String &barcode) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Suche Produkt...");
    textCenter("Open Food Facts", W / 2,  80, 1, COLOR_SUBTEXT);
    String b = barcode.length() > 24 ? barcode.substring(0, 24) : barcode;
    textCenter(b, W / 2, 116, 2, COLOR_ACCENT);
    static uint8_t dots = 0;
    String d = ""; for (int i = 0; i < (dots % 4); i++) d += " .";
    _gfx->fillRect(0, 144, W, 30, COLOR_BG);
    textCenter(d, W / 2, 156, 2, COLOR_TEXT); dots++;
    _gfx->flush();
}

// ── Datumseingabe (Numpad-Layout) ─────────────────────────────

void DisplayManager::showDateEntry(const DateInput &d, const String &productName,
                                    bool wifiOk, int dragCol, int16_t dragPx) {
    _gfx->fillScreen(COLOR_BG);

    // ── Produktname ──────────────────────────────────────────────
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn, W / 2, DRUM_HDR_H / 2, 2, COLOR_TEXT);

    // ── Spalten-Trennlinien ──────────────────────────────────────
    _gfx->drawFastVLine(    DRUM_COL_W, DRUM_HDR_H, DRUM_BTN_Y - DRUM_HDR_H, COLOR_SURFACE);
    _gfx->drawFastVLine(2 * DRUM_COL_W, DRUM_HDR_H, DRUM_BTN_Y - DRUM_HDR_H, COLOR_SURFACE);

    // ── ▲ Pfeil-Buttons ──────────────────────────────────────────
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

    // ── Drum-Zahlen ──────────────────────────────────────────────
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
            uint16_t clr;
            uint8_t  sz;
            if      (dist10 <  5) { sz = 3; clr = COLOR_TEXT;    }
            else if (dist10 < 15) { sz = 2; clr = COLOR_SUBTEXT; }
            else                  { sz = 1; clr = 0x2104;        }

            if (rowCY < DRUM_TOP + 4 * sz || rowCY > DRUM_ARRDWN_Y - 4 * sz) continue;

            String valStr = (c == 2) ? String(val)
                                     : (val < 10 ? "0" + String(val) : String(val));
            textCenter(valStr, colCX, rowCY, sz, clr);
        }
    }

    // ── Auswahlrahmen ────────────────────────────────────────────
    _gfx->drawRoundRect(2, DRUM_SEL_Y, W - 4, DRUM_ROW_H, 6, COLOR_ACCENT);

    // ── ▼ Pfeil-Buttons ──────────────────────────────────────────
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

    // ── Back + OK Buttons ────────────────────────────────────────
    int16_t btnY = DRUM_BTN_Y + 4;
    int16_t btnH = H - DRUM_BTN_Y - 8;
    int16_t halfW = W / 2 - 6;
    // Back (links, rot, nur Rahmen)
    _gfx->drawRoundRect(4, btnY, halfW, btnH, 8, COLOR_DANGER);
    textCenter("< Zuruck", 4 + halfW / 2, btnY + btnH / 2, 1, COLOR_DANGER);
    // OK (rechts, grün, nur Rahmen)
    _gfx->drawRoundRect(W / 2 + 2, btnY, halfW, btnH, 8, COLOR_OK);
    textCenter("OK", W / 2 + 2 + halfW / 2, btnY + btnH / 2, 2, COLOR_OK);

    _gfx->flush();
}

// ── Drucken ───────────────────────────────────────────────────

void DisplayManager::showPrinting() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Drucke Etikett...", COLOR_ACCENT);
    // Drucker-Icon (einfaches Rechteck + Papier)
    int16_t px = W / 2 - 36, py = 70;
    _gfx->fillRoundRect(px, py,      72, 50, 6, COLOR_SURFACE);   // Gehäuse
    _gfx->fillRect     (px + 8, py - 20, 56, 26, COLOR_SUBTEXT); // Papiereinzug
    _gfx->fillRect     (px + 14, py + 26, 44, 32, 0xFFFF);        // Papier-Ausgabe
    // Drucklinien auf dem Papier
    for (int l = 0; l < 3; l++)
        _gfx->fillRect(px + 18, py + 30 + l * 9, 36, 4, COLOR_SURFACE);
    textCenter("Bitte warten...", W / 2, 158, 1, COLOR_SUBTEXT);
    _gfx->flush();
}

// ── Erfolg / Fehler ───────────────────────────────────────────

void DisplayManager::showSuccess(const String &productName, const String &date, bool showReprint) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Eingelagert!", COLOR_BTN_OK);
    _gfx->fillCircle(W / 2, 116, 44, COLOR_OK);
    for (int t = 0; t < 5; t++) {
        _gfx->drawLine(W/2-24+t, 116, W/2-4+t,  137, COLOR_BG);
        _gfx->drawLine(W/2-4+t,  137, W/2+24+t,  97, COLOR_BG);
    }
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn,           W / 2, 178, 2, COLOR_TEXT);
    textCenter("MHD: "+date, W / 2, 202, 1, COLOR_SUBTEXT);
    if (showReprint) {
        // Zwei Buttons unten: Nochmal drucken | Weiter
        drawTouchButton(TBTN_X,            228, (TBTN_W - 4) / 2, TBTN_H,
                        "Nochmal", COLOR_BTN_BACK, COLOR_TEXT, 1);
        drawTouchButton(TBTN_X + (TBTN_W + 4) / 2, 228, (TBTN_W - 4) / 2, TBTN_H,
                        "Weiter",  COLOR_BTN_OK,   COLOR_TEXT);
    }
    _gfx->flush();
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);
    _gfx->fillCircle(W / 2, 128, 46, COLOR_DANGER);
    textCenter("!", W / 2, 116, 4, COLOR_BG, COLOR_DANGER);
    textCenter(msg, W / 2, 196, 1, COLOR_TEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H, "OK", COLOR_BTN_BACK, COLOR_TEXT);
    _gfx->flush();
}

// ── Auslagerung (Label-Barcode gescannt) ──────────────────────

void DisplayManager::showRetrieve(const String &name, const String &storageDate,
                                   const String &expiryDate, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Auslagerung", COLOR_ACCENT);
    String n = name.length() > 28 ? name.substring(0, 28) : name;
    textCenter(n, W / 2, 72, 2, COLOR_TEXT);
    textCenter("Eingelagert: " + storageDate, W / 2, 106, 1, COLOR_SUBTEXT);
    textCenter("MHD: " + expiryDate,          W / 2, 126, 1, COLOR_SUBTEXT);
    uint16_t sc = (daysLeft < 0)             ? COLOR_DANGER
                : (daysLeft <= DANGER_DAYS)  ? COLOR_DANGER
                : (daysLeft <= WARNING_DAYS) ? COLOR_WARN
                :                              COLOR_OK;
    textCenter(daysLabel(daysLeft), W / 2, 164, 3, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W / 2, 196, 1, COLOR_SUBTEXT);
    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, TBTN_H,
                    "Behalten", COLOR_SURFACE, COLOR_SUBTEXT, 1);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Auslagern", COLOR_BTN_OK, COLOR_TEXT);
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
    textCenter(String(index + 1) + " / " + String(total), W / 2, HDR / 2 + 3, 2, 0xFFFF, COLOR_HEADER);

    int16_t y = HDR + 14;
    String n = name.length() > 26 ? name.substring(0, 26) : name;
    textLeft(n, 10, y, 2, COLOR_TEXT); y += 28;
    if (name.length() > 26) {
        textLeft(name.substring(26, 52), 10, y, 2, COLOR_TEXT); y += 28;
    }
    y += 6;
    textLeft("MHD:   " + expiry,                10, y,      1, COLOR_SUBTEXT);
    textLeft("Menge: " + String(qty) + " Stk.", 10, y + 22, 1, COLOR_SUBTEXT);

    textCenter(daysLabel(daysLeft), W * 3 / 4, 122, 3, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W * 3 / 4, 154, 1, COLOR_SUBTEXT);

    textCenter("< wischen >", W / 2, 162, 1, COLOR_SURFACE);
    drawTouchButton(TBTN_X, INV_BACK_Y, TBTN_W, TBTN_H, "Zurueck",          COLOR_SURFACE, COLOR_SUBTEXT, 1);
    drawTouchButton(TBTN_X, INV_DEL_Y,  TBTN_W, TBTN_H, "Artikel loeschen", COLOR_DANGER,  COLOR_TEXT);
    _gfx->flush();
}

// ── AP-Modus ──────────────────────────────────────────────────

void DisplayManager::showAPMode(const String &ssid, const String &password, const String &ip) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WiFi Einrichtung", COLOR_WARN);
    textCenter("Kein WLAN konfiguriert!",    W / 2,  60, 1, COLOR_WARN);
    textCenter("Netz:",                      W / 2,  80, 1, COLOR_SUBTEXT);
    textCenter(ssid,                         W / 2, 106, 2, COLOR_ACCENT);
    textCenter("PW:",                        W / 2, 128, 1, COLOR_SUBTEXT);
    textCenter(password,                     W / 2, 152, 2, COLOR_TEXT);
    _gfx->drawFastHLine(20, 170, W - 40, COLOR_SURFACE);
    textCenter("Browser: http://" + ip,      W / 2, 188, 1, COLOR_ACCENT);
    drawTouchButton(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H,
                    "Zur Produktliste", COLOR_BTN_OK, COLOR_TEXT);
    drawTouchButton(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H,
                    "Inventar anzeigen", COLOR_BTN_BACK, COLOR_TEXT);
    _gfx->flush();
}
