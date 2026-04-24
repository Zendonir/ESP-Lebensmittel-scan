#include "DisplayManager.h"

static constexpr int16_t W   = DISPLAY_W;   // 280
static constexpr int16_t H   = DISPLAY_H;   // 456
static constexpr int16_t HDR = 56;

// ── Init ──────────────────────────────────────────────────────

DisplayManager::DisplayManager() : _bus(nullptr), _gfx(nullptr) {}

bool DisplayManager::begin() {
    _bus = new Arduino_QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    _gfx = new Arduino_RM67162(_bus, LCD_RST, 0, false, W, H);

    if (!_gfx->begin()) return false;
    _gfx->fillScreen(COLOR_BG);
    _gfx->setBrightness(220);
    return true;
}

void DisplayManager::setBrightness(uint8_t val) {
    if (_gfx) _gfx->setBrightness(val);
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

// ── Layout-Bausteine ──────────────────────────────────────────

void DisplayManager::drawHeader(const String &title, uint16_t bg) {
    _gfx->fillRect(0, 0, W, HDR, bg);
    textCenter(title, W / 2, HDR / 2, 2, 0xFFFF, bg);
}

void DisplayManager::drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                      const String &label, uint16_t bg, uint16_t fg,
                                      uint8_t textSz) {
    _gfx->fillRoundRect(x, y, w, h, 12, bg);
    textCenter(label, x + w / 2, y + h / 2, textSz, fg, bg);
}

void DisplayManager::drawPlusMinusColumn(int col, int value, bool isYear,
                                          bool plusActive, bool minusActive) {
    int16_t cx = col * DATE_COL_W + DATE_COL_W / 2;

    // Plus-Zone
    uint16_t plusBg = plusActive ? COLOR_SELECTED : COLOR_SURFACE;
    _gfx->fillRoundRect(col * DATE_COL_W + 3, DATE_PLUS_Y0, DATE_COL_W - 6,
                         DATE_PLUS_Y1 - DATE_PLUS_Y0, 8, plusBg);
    textCenter("+", cx, DATE_PLUS_Y0 + (DATE_PLUS_Y1 - DATE_PLUS_Y0) / 2,
               3, COLOR_OK, plusBg);

    // Wert-Zone (dunklerer Hintergrund)
    _gfx->fillRect(col * DATE_COL_W + 3, DATE_VAL_Y0, DATE_COL_W - 6,
                   DATE_VAL_Y1 - DATE_VAL_Y0, COLOR_BG);
    char buf[6];
    if (isYear)
        snprintf(buf, sizeof(buf), "%d", value);
    else
        snprintf(buf, sizeof(buf), "%02d", value);
    textCenter(buf, cx, DATE_VAL_Y0 + (DATE_VAL_Y1 - DATE_VAL_Y0) / 2,
               3, COLOR_TEXT, COLOR_BG);

    // Minus-Zone
    uint16_t minusBg = minusActive ? COLOR_SELECTED : COLOR_SURFACE;
    _gfx->fillRoundRect(col * DATE_COL_W + 3, DATE_MINUS_Y0, DATE_COL_W - 6,
                         DATE_MINUS_Y1 - DATE_MINUS_Y0, 8, minusBg);
    textCenter("-", cx, DATE_MINUS_Y0 + (DATE_MINUS_Y1 - DATE_MINUS_Y0) / 2,
               3, COLOR_DANGER, minusBg);

    // Trenn-Linie zwischen Spalten
    if (col < 2) {
        _gfx->drawFastVLine(col * DATE_COL_W + DATE_COL_W,
                             DATE_PLUS_Y0, DATE_MINUS_Y1 - DATE_PLUS_Y0, COLOR_BG);
    }
}

String DisplayManager::daysLabel(int days) {
    if (days < 0)  return "ABGELAUFEN";
    if (days == 0) return "Heute!";
    if (days == 1) return "1 Tag";
    return String(days) + " Tage";
}

// ── Bildschirme ───────────────────────────────────────────────

void DisplayManager::showBooting(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    textCenter("Lebensmittel", W / 2, H / 2 - 50, 2, COLOR_ACCENT);
    textCenter("Scanner",      W / 2, H / 2 - 26, 2, COLOR_ACCENT);
    _gfx->drawFastHLine(40, H / 2 + 2, W - 80, COLOR_SURFACE);
    if (!msg.isEmpty())
        textCenter(msg, W / 2, H / 2 + 28, 1, COLOR_SUBTEXT);
}

void DisplayManager::showWifiConnecting(const String &ssid, int attempt) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WLAN Verbindung");

    textCenter("Verbinde mit:",        W / 2, 90,  1, COLOR_SUBTEXT);
    textCenter(ssid,                   W / 2, 120, 2, COLOR_ACCENT);

    int barW   = W - 40;
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(20, 158, barW, 12, COLOR_SURFACE);
    if (filled > 0) _gfx->fillRect(20, 158, filled, 12, COLOR_ACCENT);

    textCenter("Bitte warten...", W / 2, 196, 1, COLOR_SUBTEXT);
}

void DisplayManager::showIdle(int total, int expiring, int expired) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bereit");

    // Statistik-Kacheln (3 Stück, gleichmäßig verteilt)
    struct { const char *l; int v; uint16_t c; int16_t y; } tiles[] = {
        { "Gesamt",      total,    COLOR_TEXT,   100 },
        { "Bald weg",    expiring, COLOR_WARN,   200 },
        { "Abgelaufen",  expired,  COLOR_DANGER, 300 },
    };
    for (auto &t : tiles) {
        _gfx->fillRoundRect(20, t.y - 36, W - 40, 72, 10, COLOR_SURFACE);
        textCenter(t.l,           W / 2, t.y - 18, 1, COLOR_SUBTEXT, COLOR_SURFACE);
        textCenter(String(t.v),   W / 2, t.y + 16, 3,
                   t.v > 0 ? t.c : COLOR_TEXT, COLOR_SURFACE);
    }

    // Hinweis
    textCenter("Barcode scannen", W / 2, 380, 1, COLOR_SUBTEXT);

    // Inventar-Button
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Inventar anzeigen", COLOR_BTN_BACK, COLOR_TEXT);
}

void DisplayManager::showScanning() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");

    // Barcode-Symbol
    int bx = 30, by = 80, bw = W - 60, bh = 110;
    for (int x = 0; x < bw; x += 7) {
        _gfx->fillRect(bx + x, by, (x % 14 < 7) ? 4 : 2, bh, COLOR_TEXT);
    }
    _gfx->fillRect(bx, by + bh + 6, bw, 2, COLOR_SUBTEXT);

    textCenter("GM861 vor den Scanner halten", W / 2, 235, 1, COLOR_SUBTEXT);
    textCenter("EAN13 • EAN8 • QR • DataMatrix", W / 2, 255, 1, COLOR_SUBTEXT);

    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Abbrechen", COLOR_SURFACE, COLOR_TEXT);
}

void DisplayManager::showFetching(const String &barcode) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Suche Produkt...");

    textCenter("Open Food Facts", W / 2, 100, 1, COLOR_SUBTEXT);

    String b = barcode.length() > 18 ? barcode.substring(0, 18) : barcode;
    textCenter(b, W / 2, 140, 2, COLOR_ACCENT);

    static uint8_t dots = 0;
    String d = "";
    for (int i = 0; i < (dots % 4); i++) d += " .";
    _gfx->fillRect(0, 190, W, 36, COLOR_BG);
    textCenter(d, W / 2, 200, 2, COLOR_TEXT);
    dots++;
}

void DisplayManager::showProduct(const ProductInfo &info, int stock) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader(info.found ? "Produkt gefunden" : "Unbekanntes Produkt");

    // Produktname (bis 2 Zeilen)
    String name = info.name.isEmpty() ? ("ID: " + info.barcode) : info.name;
    int16_t y = 68;
    if (name.length() <= 22) {
        textLeft(name, 10, y, 2, COLOR_TEXT);
        y += 22;
    } else {
        textLeft(name.substring(0, 22), 10, y,      2, COLOR_TEXT);
        textLeft(name.substring(22, 44), 10, y + 22, 2, COLOR_TEXT);
        y += 44;
    }
    y += 12;

    _gfx->drawFastHLine(10, y, W - 20, COLOR_SURFACE);
    y += 12;

    if (!info.brand.isEmpty()) {
        textLeft("Marke:   " + info.brand,    10, y, 1, COLOR_SUBTEXT);
        y += 20;
    }
    if (!info.quantity.isEmpty()) {
        textLeft("Inhalt:  " + info.quantity, 10, y, 1, COLOR_SUBTEXT);
        y += 20;
    }
    if (!info.category.isEmpty()) {
        textLeft("Kat.:    " + info.category, 10, y, 1, COLOR_SUBTEXT);
        y += 20;
    }
    uint16_t stockColor = stock > 0 ? COLOR_WARN : COLOR_SUBTEXT;
    textLeft("Im Lager: " + String(stock) + " Stk.", 10, y, 1, stockColor);

    // Touch-Buttons
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y,   TBTN_W, TBTN_H,
                    "Hinzufuegen", COLOR_BTN_OK,   COLOR_TEXT);
    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, 40,
                    "Abbrechen",  COLOR_SURFACE, COLOR_SUBTEXT, 1);
}

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Haltbarkeitsdatum");

    // Produktname (eine Zeile)
    String pn = productName.length() > 32 ? productName.substring(0, 32) : productName;
    textCenter(pn, W / 2, 70, 1, COLOR_SUBTEXT);

    // Spalten-Beschriftungen
    const char *labels[] = { "Tag", "Mon", "Jahr" };
    for (int i = 0; i < 3; i++) {
        textCenter(labels[i], i * DATE_COL_W + DATE_COL_W / 2, 82, 1,
                   COLOR_SUBTEXT, COLOR_BG);
    }

    // +/- Spalten
    drawPlusMinusColumn(0, d.day,   false, false, false);
    drawPlusMinusColumn(1, d.month, false, false, false);
    drawPlusMinusColumn(2, d.year,  true,  false, false);

    // Spalten-Trennlinien
    for (int i = 1; i < 3; i++) {
        _gfx->drawFastVLine(i * DATE_COL_W, DATE_PLUS_Y0, DATE_MINUS_Y1 - DATE_PLUS_Y0, COLOR_SURFACE);
    }

    // Datum-Vorschau
    char preview[12];
    snprintf(preview, sizeof(preview), "%02d.%02d.%04d", d.day, d.month, d.year);
    _gfx->fillRoundRect(20, DATE_MINUS_Y1 + 6, W - 40, 36, 8, COLOR_SURFACE);
    textCenter(preview, W / 2, DATE_MINUS_Y1 + 24, 2, COLOR_ACCENT, COLOR_SURFACE);

    // Touch-Buttons
    drawTouchButton(TBTN_X, DATE_OK_Y,   TBTN_W, TBTN_H,
                    "Bestaetigen", COLOR_BTN_OK, COLOR_TEXT);
    drawTouchButton(TBTN_X, DATE_BACK_Y, TBTN_W, 40,
                    "Abbrechen",  COLOR_SURFACE, COLOR_SUBTEXT, 1);
}

void DisplayManager::showSuccess(const String &productName, const String &date) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Hinzugefuegt!", COLOR_BTN_OK);

    // Grosser grüner Kreis mit Häkchen
    _gfx->fillCircle(W / 2, 160, 55, COLOR_OK);
    for (int t = 0; t < 5; t++) {
        _gfx->drawLine(W/2 - 28 + t, 160,  W/2 - 8 + t, 180, COLOR_BG);
        _gfx->drawLine(W/2 - 8 + t,  180,  W/2 + 26 + t, 130, COLOR_BG);
    }

    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn,           W / 2, 248, 2, COLOR_TEXT);
    textCenter("MHD: "+date, W / 2, 278, 1, COLOR_SUBTEXT);

    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Weiter", COLOR_BTN_BACK, COLOR_TEXT);
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);

    _gfx->fillCircle(W / 2, 160, 50, COLOR_DANGER);
    textCenter("!", W / 2, 148, 4, COLOR_BG, COLOR_DANGER);

    textCenter(msg, W / 2, 250, 1, COLOR_TEXT);

    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "OK", COLOR_BTN_BACK, COLOR_TEXT);
}

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);

    // Farbstreifen oben
    uint16_t sc = (daysLeft < 0 || daysLeft <= DANGER_DAYS) ? COLOR_DANGER
                : (daysLeft <= WARNING_DAYS)                  ? COLOR_WARN
                :                                               COLOR_OK;
    _gfx->fillRect(0, 0, W, 8, sc);

    // Header
    _gfx->fillRect(0, 8, W, HDR - 8, COLOR_HEADER);
    textCenter(String(index + 1) + " / " + String(total),
               W / 2, HDR / 2 + 4, 2, 0xFFFF, COLOR_HEADER);

    // Produktname (2 Zeilen möglich)
    String n = name;
    int16_t y = HDR + 16;
    if (n.length() <= 22) {
        textLeft(n, 10, y, 2, COLOR_TEXT);
        y += 24;
    } else {
        textLeft(n.substring(0, 22), 10, y,      2, COLOR_TEXT);
        textLeft(n.substring(22, 44), 10, y + 24, 2, COLOR_TEXT);
        y += 50;
    }

    y += 10;
    textLeft("MHD:   " + expiry,          10, y,      1, COLOR_SUBTEXT);
    textLeft("Menge: " + String(qty) + " Stk.", 10, y + 20, 1, COLOR_SUBTEXT);

    _gfx->drawFastHLine(10, y + 44, W - 20, COLOR_SURFACE);

    // Grosse Status-Anzeige
    textCenter(daysLabel(daysLeft), W / 2, 260, 3, sc);
    if (daysLeft >= 0)
        textCenter("verbleibend", W / 2, 300, 1, COLOR_SUBTEXT);

    // Wischen-Hinweis
    textCenter("< wischen zum Blaettern >", W / 2, 330, 1, COLOR_SURFACE);

    // Touch-Buttons
    drawTouchButton(TBTN_X, INV_DEL_Y,  TBTN_W, TBTN_H,
                    "Artikel loeschen", COLOR_DANGER, COLOR_TEXT);
    drawTouchButton(TBTN_X, INV_BACK_Y, TBTN_W, 36,
                    "Zurueck",          COLOR_SURFACE, COLOR_SUBTEXT, 1);
}
