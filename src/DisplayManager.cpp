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

// ── Bausteine ─────────────────────────────────────────────────

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

void DisplayManager::drawPlusMinusColumn(int col, int value, bool isYear) {
    int16_t cx   = col * DATE_COL_W + DATE_COL_W / 2;
    int16_t colX = col * DATE_COL_W + 3;
    int16_t colW = DATE_COL_W - 6;

    // Plus-Zone
    _gfx->fillRoundRect(colX, DATE_PLUS_Y0, colW, DATE_PLUS_Y1 - DATE_PLUS_Y0, 8, COLOR_SURFACE);
    textCenter("+", cx, DATE_PLUS_Y0 + (DATE_PLUS_Y1 - DATE_PLUS_Y0) / 2, 3, COLOR_OK, COLOR_SURFACE);

    // Wert
    _gfx->fillRect(colX, DATE_VAL_Y0, colW, DATE_VAL_Y1 - DATE_VAL_Y0, COLOR_BG);
    char buf[6];
    if (isYear) snprintf(buf, sizeof(buf), "%d", value);
    else        snprintf(buf, sizeof(buf), "%02d", value);
    textCenter(buf, cx, DATE_VAL_Y0 + (DATE_VAL_Y1 - DATE_VAL_Y0) / 2, 3, COLOR_TEXT, COLOR_BG);

    // Minus-Zone
    _gfx->fillRoundRect(colX, DATE_MINUS_Y0, colW, DATE_MINUS_Y1 - DATE_MINUS_Y0, 8, COLOR_SURFACE);
    textCenter("-", cx, DATE_MINUS_Y0 + (DATE_MINUS_Y1 - DATE_MINUS_Y0) / 2, 3, COLOR_DANGER, COLOR_SURFACE);

    // Trennlinie zwischen Spalten
    if (col < 2) {
        _gfx->drawFastVLine((col + 1) * DATE_COL_W,
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
    textCenter("Verbinde mit:",  W / 2,  90, 1, COLOR_SUBTEXT);
    textCenter(ssid,             W / 2, 120, 2, COLOR_ACCENT);
    int barW   = W - 40;
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(20, 158, barW, 12, COLOR_SURFACE);
    if (filled > 0) _gfx->fillRect(20, 158, filled, 12, COLOR_ACCENT);
    textCenter("Bitte warten...", W / 2, 196, 1, COLOR_SUBTEXT);
}

void DisplayManager::showIdle(int total, int expiring, int expired, int customCount) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bereit");

    // Kompaktere Statistik-Kacheln (3 Stück, schmaler)
    struct { const char *l; int v; uint16_t c; int16_t y; } tiles[] = {
        { "Gesamt",      total,    COLOR_TEXT,   88  },
        { "Bald weg",    expiring, COLOR_WARN,   168 },
        { "Abgelaufen",  expired,  COLOR_DANGER, 248 },
    };
    for (auto &t : tiles) {
        _gfx->fillRoundRect(16, t.y - 32, W - 32, 62, 10, COLOR_SURFACE);
        textCenter(t.l,           W / 2, t.y - 16, 1, COLOR_SUBTEXT, COLOR_SURFACE);
        textCenter(String(t.v),   W / 2, t.y + 14, 3,
                   t.v > 0 ? t.c : COLOR_TEXT, COLOR_SURFACE);
    }

    // Aktions-Buttons
    drawTouchButton(TBTN_X, IDLE_LIST_BTN_Y, TBTN_W, IDLE_BTN_H,
                    customCount > 0 ? "Aus Liste waehlen" : "Produkt-Liste (leer)",
                    customCount > 0 ? COLOR_BTN_OK : COLOR_SURFACE,
                    COLOR_TEXT);

    drawTouchButton(TBTN_X, IDLE_INV_BTN_Y, TBTN_W, IDLE_BTN_H,
                    "Inventar anzeigen", COLOR_BTN_BACK, COLOR_TEXT);

    // Scan-Hinweis
    textCenter("oder Barcode mit GM861 scannen", W / 2, 434, 1, COLOR_SUBTEXT);
}

void DisplayManager::showScanning() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");
    int bx = 30, by = 80, bw = W - 60, bh = 110;
    for (int x = 0; x < bw; x += 7)
        _gfx->fillRect(bx + x, by, (x % 14 < 7) ? 4 : 2, bh, COLOR_TEXT);
    _gfx->fillRect(bx, by + bh + 6, bw, 2, COLOR_SUBTEXT);
    textCenter("GM861 vor den Scanner halten",    W / 2, 235, 1, COLOR_SUBTEXT);
    textCenter("EAN13 • EAN8 • QR • DataMatrix",  W / 2, 255, 1, COLOR_SUBTEXT);
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

    // Header mit Produktname prominent oben
    _gfx->fillRect(0, 0, W, HDR + 28, COLOR_HEADER);
    textCenter("Produkt gefunden", W / 2, 14, 1, COLOR_SUBTEXT, COLOR_HEADER);
    // Produktname – bis 22 Zeichen in Größe 2
    String n = info.name.isEmpty() ? ("ID: " + info.barcode) : info.name;
    if (n.length() > 22) n = n.substring(0, 22);
    textCenter(n, W / 2, 50, 2, COLOR_TEXT, COLOR_HEADER);

    int16_t y = HDR + 38;
    _gfx->drawFastHLine(10, y, W - 20, COLOR_SURFACE);
    y += 12;

    if (!info.brand.isEmpty()) {
        textLeft("Marke:   " + info.brand,    10, y, 1, COLOR_SUBTEXT); y += 20;
    }
    if (!info.quantity.isEmpty()) {
        textLeft("Inhalt:  " + info.quantity, 10, y, 1, COLOR_SUBTEXT); y += 20;
    }
    if (!info.category.isEmpty()) {
        textLeft("Kat.:    " + info.category, 10, y, 1, COLOR_SUBTEXT); y += 20;
    }
    textLeft("Im Lager: " + String(stock) + " Stk.", 10, y, 1,
             stock > 0 ? COLOR_WARN : COLOR_SUBTEXT);

    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y,   TBTN_W, TBTN_H,
                    "Hinzufuegen", COLOR_BTN_OK, COLOR_TEXT);
    drawTouchButton(TBTN_X, TBTN_SECONDARY_Y, TBTN_W, 40,
                    "Abbrechen",  COLOR_SURFACE, COLOR_SUBTEXT, 1);
}

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _gfx->fillScreen(COLOR_BG);

    // Erweiterter Header: Titel + Produktname prominent
    _gfx->fillRect(0, 0, W, HDR + 26, COLOR_HEADER);
    textCenter("Haltbarkeitsdatum", W / 2, 14, 1, COLOR_SUBTEXT, COLOR_HEADER);
    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn, W / 2, 48, 2, COLOR_TEXT, COLOR_HEADER);

    // Spalten-Beschriftungen
    const char *labels[] = { "Tag", "Mon", "Jahr" };
    for (int i = 0; i < 3; i++)
        textCenter(labels[i], i * DATE_COL_W + DATE_COL_W / 2, 82, 1, COLOR_SUBTEXT);

    // +/- Spalten
    drawPlusMinusColumn(0, d.day,   false);
    drawPlusMinusColumn(1, d.month, false);
    drawPlusMinusColumn(2, d.year,  true);

    for (int i = 1; i < 3; i++)
        _gfx->drawFastVLine(i * DATE_COL_W, DATE_PLUS_Y0, DATE_MINUS_Y1 - DATE_PLUS_Y0, COLOR_BG);

    // Datum-Vorschau
    char preview[12];
    snprintf(preview, sizeof(preview), "%02d.%02d.%04d", d.day, d.month, d.year);
    _gfx->fillRoundRect(20, DATE_MINUS_Y1 + 6, W - 40, 36, 8, COLOR_SURFACE);
    textCenter(preview, W / 2, DATE_MINUS_Y1 + 24, 2, COLOR_ACCENT, COLOR_SURFACE);

    drawTouchButton(TBTN_X, DATE_OK_Y,   TBTN_W, TBTN_H, "Bestaetigen", COLOR_BTN_OK, COLOR_TEXT);
    drawTouchButton(TBTN_X, DATE_BACK_Y, TBTN_W, 40,     "Abbrechen",   COLOR_SURFACE, COLOR_SUBTEXT, 1);
}

void DisplayManager::showProductList(const std::vector<CustomProduct> &products, int offset) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Produkt waehlen");

    if (products.empty()) {
        textCenter("Keine Vorlagen definiert",   W / 2, 180, 2, COLOR_SUBTEXT);
        textCenter("Im Web-Interface hinzufuegen", W / 2, 220, 1, COLOR_SUBTEXT);
        drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                        "Zurueck", COLOR_SURFACE, COLOR_TEXT);
        return;
    }

    // Liste zeichnen (alternierend eingefärbt)
    int visible = min((int)LIST_MAX_VIS, (int)products.size() - offset);
    for (int i = 0; i < visible; i++) {
        const auto &p   = products[offset + i];
        int16_t itemY   = HDR + i * LIST_ITEM_H;
        uint16_t rowBg  = (i % 2 == 0) ? COLOR_BG : COLOR_SURFACE;

        _gfx->fillRect(0, itemY, W, LIST_ITEM_H, rowBg);

        // Produktname (Zeile 1, Größe 2)
        String name = p.name.length() > 20 ? p.name.substring(0, 20) : p.name;
        textLeft(name, 10, itemY + 4, 2, COLOR_TEXT, rowBg);

        // Marke (Zeile 2, Größe 1)
        if (!p.brand.isEmpty()) {
            String brand = p.brand.length() > 30 ? p.brand.substring(0, 30) : p.brand;
            textLeft(brand, 10, itemY + 28, 1, COLOR_SUBTEXT, rowBg);
        }

        // Trenn-Linie
        _gfx->drawFastHLine(0, itemY + LIST_ITEM_H - 1, W, COLOR_SURFACE);
    }

    // Scroll-Indikator (rechts, kleine Leiste)
    if ((int)products.size() > LIST_MAX_VIS) {
        int total = products.size();
        int barH  = LIST_MAX_VIS * LIST_ITEM_H;
        int markH = max(8, barH * LIST_MAX_VIS / total);
        int markY = HDR + (barH - markH) * offset / max(1, total - LIST_MAX_VIS);
        _gfx->fillRect(W - 4, HDR, 4, barH, COLOR_SURFACE);
        _gfx->fillRect(W - 4, markY, 4, markH, COLOR_ACCENT);

        // Wisch-Hinweis
        textCenter("< wischen zum Scrollen >", W / 2 - 2, LIST_BACK_BTN_Y - 18, 1, COLOR_SURFACE);
    }

    // Zurück-Button ganz unten
    drawTouchButton(TBTN_X, LIST_BACK_BTN_Y, TBTN_W, 40,
                    "Zurueck", COLOR_SURFACE, COLOR_SUBTEXT, 1);
}

void DisplayManager::showSuccess(const String &productName, const String &date) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Hinzugefuegt!", COLOR_BTN_OK);
    _gfx->fillCircle(W / 2, 155, 55, COLOR_OK);
    for (int t = 0; t < 5; t++) {
        _gfx->drawLine(W/2 - 28 + t, 155,  W/2 - 8 + t, 175, COLOR_BG);
        _gfx->drawLine(W/2 - 8 + t,  175,  W/2 + 26 + t, 125, COLOR_BG);
    }
    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn,           W / 2, 245, 2, COLOR_TEXT);
    textCenter("MHD: "+date, W / 2, 275, 1, COLOR_SUBTEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "Weiter", COLOR_BTN_BACK, COLOR_TEXT);
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);
    _gfx->fillCircle(W / 2, 155, 50, COLOR_DANGER);
    textCenter("!", W / 2, 143, 4, COLOR_BG, COLOR_DANGER);
    textCenter(msg, W / 2, 248, 1, COLOR_TEXT);
    drawTouchButton(TBTN_X, TBTN_PRIMARY_Y, TBTN_W, TBTN_H,
                    "OK", COLOR_BTN_BACK, COLOR_TEXT);
}

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);
    uint16_t sc = (daysLeft < 0 || daysLeft <= DANGER_DAYS) ? COLOR_DANGER
                : (daysLeft <= WARNING_DAYS)                  ? COLOR_WARN
                :                                               COLOR_OK;
    _gfx->fillRect(0, 0, W, 8, sc);
    _gfx->fillRect(0, 8, W, HDR - 8, COLOR_HEADER);
    textCenter(String(index + 1) + " / " + String(total),
               W / 2, HDR / 2 + 4, 2, 0xFFFF, COLOR_HEADER);

    // Produktname prominent oben
    String n = name.length() > 22 ? name.substring(0, 22) : name;
    int16_t y = HDR + 16;
    textLeft(n, 10, y, 2, COLOR_TEXT);
    if (name.length() > 22) {
        String n2 = name.substring(22, 44);
        textLeft(n2, 10, y + 24, 2, COLOR_TEXT);
        y += 50;
    } else {
        y += 26;
    }

    y += 8;
    textLeft("MHD:   " + expiry,                  10, y,      1, COLOR_SUBTEXT);
    textLeft("Menge: " + String(qty) + " Stk.",   10, y + 20, 1, COLOR_SUBTEXT);
    _gfx->drawFastHLine(10, y + 44, W - 20, COLOR_SURFACE);

    textCenter(daysLabel(daysLeft), W / 2, 258, 3, sc);
    if (daysLeft >= 0)
        textCenter("verbleibend", W / 2, 298, 1, COLOR_SUBTEXT);

    textCenter("< wischen zum Blaettern >", W / 2, 330, 1, COLOR_SURFACE);

    drawTouchButton(TBTN_X, INV_DEL_Y,  TBTN_W, TBTN_H,
                    "Artikel loeschen", COLOR_DANGER, COLOR_TEXT);
    drawTouchButton(TBTN_X, INV_BACK_Y, TBTN_W, 36,
                    "Zurueck",          COLOR_SURFACE, COLOR_SUBTEXT, 1);
}
