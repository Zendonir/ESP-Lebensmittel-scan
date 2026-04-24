#include "DisplayManager.h"

// ── Layout-Konstanten ──────────────────────────────────────────
static constexpr int16_t W   = DISPLAY_W;
static constexpr int16_t H   = DISPLAY_H;
static constexpr int16_t HDR = 56;   // Header-Höhe
static constexpr int16_t FTR = 44;   // Footer-Höhe
static constexpr int16_t CTH = H - HDR - FTR;  // Content-Höhe
static constexpr int16_t CY0 = HDR;             // Content-Start Y
static constexpr int16_t FY0 = H - FTR;         // Footer-Start Y

// ── Konstruktor / Initialisierung ──────────────────────────────

DisplayManager::DisplayManager() : _bus(nullptr), _gfx(nullptr) {}

bool DisplayManager::begin() {
    _bus = new Arduino_QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    // RM67162 – 280×456 Portrait
    _gfx = new Arduino_RM67162(_bus, LCD_RST, 0 /*rotation*/, false /*IPS*/, W, H);

    if (!_gfx->begin()) return false;

    _gfx->fillScreen(COLOR_BG);
    _gfx->setBrightness(220);
    return true;
}

void DisplayManager::setBrightness(uint8_t val) {
    if (_gfx) _gfx->setBrightness(val);
}

// ── Privater Text-Helfer ───────────────────────────────────────

int16_t DisplayManager::textWidth(const String &s, uint8_t sz) {
    // Built-in GFX Font: 6px/char Breite * sz
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

void DisplayManager::drawHeader(const String &title, uint16_t bgColor) {
    _gfx->fillRect(0, 0, W, HDR, bgColor);
    textCenter(title, W / 2, HDR / 2, 2, 0xFFFF, bgColor);
}

void DisplayManager::drawFooter(const String &left, const String &right) {
    _gfx->fillRect(0, FY0, W, FTR, COLOR_SURFACE);
    _gfx->drawFastHLine(0, FY0, W, COLOR_SUBTEXT);
    textLeft(left, 8, FY0 + 14, 1, COLOR_SUBTEXT, COLOR_SURFACE);
    if (!right.isEmpty()) {
        int16_t rx = W - 8 - textWidth(right, 1);
        textLeft(right, rx, FY0 + 14, 1, COLOR_SUBTEXT, COLOR_SURFACE);
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
    _gfx->drawFastHLine(30, H / 2 - 4, W - 60, COLOR_SURFACE);
    if (!msg.isEmpty()) textCenter(msg, W / 2, H / 2 + 20, 1, COLOR_SUBTEXT);
}

void DisplayManager::showWifiConnecting(const String &ssid, int attempt) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("WLAN Verbindung");

    textCenter("Verbinde mit:", W / 2, CY0 + 60, 1, COLOR_SUBTEXT);
    textCenter(ssid,           W / 2, CY0 + 90, 2, COLOR_ACCENT);

    // Fortschrittsbalken
    int barW = (W - 40);
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(20, CY0 + 130, barW, 12, COLOR_SURFACE);
    _gfx->fillRect(20, CY0 + 130, filled, 12, COLOR_ACCENT);

    textCenter("Bitte warten...", W / 2, CY0 + 170, 1, COLOR_SUBTEXT);
}

void DisplayManager::showIdle(int total, int expiring, int expired) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bereit");

    // ── 3 Stat-Kacheln ──
    struct { const char *label; int val; uint16_t color; int x, y; } tiles[3] = {
        { "Gesamt",      total,    COLOR_TEXT,   W/2,       CY0 + 50  },
        { "Bald weg",    expiring, COLOR_WARN,   W/2,       CY0 + 150 },
        { "Abgelaufen",  expired,  COLOR_DANGER, W/2,       CY0 + 250 },
    };

    for (auto &t : tiles) {
        uint16_t tileBg = COLOR_SURFACE;
        if (t.val > 0 && t.color != COLOR_TEXT) tileBg = COLOR_SURFACE;
        _gfx->fillRoundRect(t.x - 100, t.y - 30, 200, 70, 10, tileBg);
        textCenter(t.label,       t.x, t.y - 14, 1, COLOR_SUBTEXT, tileBg);
        textCenter(String(t.val), t.x, t.y + 18, 3, t.val > 0 ? t.color : COLOR_TEXT, tileBg);
    }

    drawFooter("Barcode scannen", "OK = Inventar");
}

void DisplayManager::showScanning() {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");

    // Barcode-Symbol zeichnen
    int bx = 30, by = CY0 + 40, bw = W - 60, bh = 110;
    for (int x = 0; x < bw; x += 7) {
        int lw = (x % 14 < 7) ? 4 : 2;
        _gfx->fillRect(bx + x, by, lw, bh, COLOR_TEXT);
    }
    _gfx->fillRect(bx, by + bh + 5, bw, 2, COLOR_SUBTEXT);

    textCenter("GM861 vor den Scanner halten", W / 2, CY0 + 210, 1, COLOR_SUBTEXT);
    textCenter("Unterstuetzt: EAN, QR, DataMatrix", W / 2, CY0 + 230, 1, COLOR_SUBTEXT);

    drawFooter("AB = Abbrechen");
}

void DisplayManager::showFetching(const String &barcode) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Suche Produkt...");

    textCenter("Open Food Facts", W / 2, CY0 + 60, 1, COLOR_SUBTEXT);

    // Barcode-Nummer
    String b = barcode.length() > 20 ? barcode.substring(0, 20) : barcode;
    textCenter(b, W / 2, CY0 + 100, 2, COLOR_ACCENT);

    // Punkte-Animation (jedes Mal andere Anzahl → muss wiederholt aufgerufen werden)
    static uint8_t dots = 0;
    String d = "";
    for (int i = 0; i < (dots % 4); i++) d += ".";
    _gfx->fillRect(0, CY0 + 150, W, 30, COLOR_BG);
    textCenter(d, W / 2, CY0 + 160, 3, COLOR_TEXT);
    dots++;

    drawFooter("Bitte warten...");
}

void DisplayManager::showProduct(const ProductInfo &info, int stock) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader(info.found ? "Produkt gefunden" : "Unbekanntes Produkt");

    // Produktname (bis 2 Zeilen à ~22 Zeichen bei Size 2)
    String name = info.name.isEmpty() ? ("Barcode: " + info.barcode) : info.name;
    int16_t y = CY0 + 20;
    if (name.length() <= 22) {
        textLeft(name.substring(0, 22), 10, y, 2, COLOR_TEXT);
    } else {
        textLeft(name.substring(0, 22), 10, y, 2, COLOR_TEXT);
        textLeft(name.substring(22, 44), 10, y + 22, 2, COLOR_TEXT);
    }

    y = CY0 + 80;
    _gfx->drawFastHLine(10, y, W - 20, COLOR_SURFACE);
    y += 10;

    if (!info.brand.isEmpty()) {
        textLeft("Marke:  " + info.brand, 10, y, 1, COLOR_SUBTEXT);
        y += 18;
    }
    if (!info.quantity.isEmpty()) {
        textLeft("Menge:  " + info.quantity, 10, y, 1, COLOR_SUBTEXT);
        y += 18;
    }
    if (!info.category.isEmpty()) {
        textLeft("Kat.:   " + info.category, 10, y, 1, COLOR_SUBTEXT);
        y += 18;
    }
    textLeft("Lager:  " + String(stock) + " Stk.", 10, y, 1,
             stock > 0 ? COLOR_OK : COLOR_SUBTEXT);

    // Buttons
    int16_t btnY = FY0 - 60;
    _gfx->fillRoundRect(10,      btnY, 120, 44, 8, COLOR_OK);
    _gfx->fillRoundRect(W - 130, btnY, 120, 44, 8, COLOR_SURFACE);
    textCenter("OK",      70,      btnY + 22, 2, COLOR_BG,  COLOR_OK);
    textCenter("Zurück",  W - 70,  btnY + 22, 2, COLOR_TEXT, COLOR_SURFACE);

    drawFooter("OK = Hinzufuegen", "AB = Zurueck");
}

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Haltbarkeitsdatum");

    // Produktname (gekürzt)
    String pn = productName.length() > 32 ? productName.substring(0, 32) : productName;
    textCenter(pn, W / 2, CY0 + 20, 1, COLOR_SUBTEXT);

    // ── 3 Felder: Tag | Monat | Jahr ──
    struct { int val; const char *label; int cx; DateField field; } fields[3] = {
        { d.day,   "Tag",   50,       FIELD_DAY   },
        { d.month, "Mon",   W / 2,    FIELD_MONTH },
        { d.year,  "Jahr",  W - 50,   FIELD_YEAR  },
    };

    for (auto &f : fields) {
        bool active = (d.activeField == f.field);
        uint16_t bg = active ? COLOR_SELECTED : COLOR_SURFACE;
        uint16_t fg = active ? COLOR_OK : COLOR_TEXT;

        _gfx->fillRoundRect(f.cx - 45, CY0 + 50, 90, 110, 10, bg);

        textCenter(f.label, f.cx, CY0 + 70, 1, COLOR_SUBTEXT, bg);

        char buf[6];
        if (f.field == FIELD_YEAR)
            snprintf(buf, sizeof(buf), "%d", f.val);
        else
            snprintf(buf, sizeof(buf), "%02d", f.val);
        textCenter(buf, f.cx, CY0 + 115, 3, fg, bg);

        if (active) {
            // Pfeil-Indikatoren
            textCenter("v", f.cx, CY0 + 50,  1, COLOR_ACCENT, bg);
            textCenter("^", f.cx, CY0 + 148, 1, COLOR_ACCENT, bg);
        }
    }

    // Datum-Vorschau
    char preview[12];
    snprintf(preview, sizeof(preview), "%02d.%02d.%04d", d.day, d.month, d.year);
    _gfx->fillRoundRect(20, CY0 + 185, W - 40, 44, 8, COLOR_SURFACE);
    textCenter(preview, W / 2, CY0 + 207, 2, COLOR_ACCENT, COLOR_SURFACE);

    textCenter("AUF/AB = aendern   OK = weiter", W / 2, CY0 + 255, 1, COLOR_SUBTEXT);

    drawFooter("AUF  AB  OK");
}

void DisplayManager::showSuccess(const String &productName, const String &date) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Hinzugefuegt!", COLOR_OK);

    // Grosses Häkchen
    _gfx->fillCircle(W / 2, CY0 + 90, 50, COLOR_OK);
    // Häkchen-Linie manuell
    for (int t = 0; t < 4; t++) {
        _gfx->drawLine(W/2 - 25 + t, CY0 + 90,  W/2 - 8 + t, CY0 + 113, COLOR_BG);
        _gfx->drawLine(W/2 - 8 + t,  CY0 + 113, W/2 + 22 + t, CY0 + 68, COLOR_BG);
    }

    String pn = productName.length() > 22 ? productName.substring(0, 22) : productName;
    textCenter(pn, W / 2, CY0 + 185, 2, COLOR_TEXT);
    textCenter("MHD: " + date, W / 2, CY0 + 215, 1, COLOR_SUBTEXT);
    textCenter("Gespeichert!", W / 2, CY0 + 245, 1, COLOR_OK);

    drawFooter("Weiter mit beliebiger Taste");
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);

    _gfx->fillCircle(W / 2, CY0 + 90, 50, COLOR_DANGER);
    textCenter("!", W / 2, CY0 + 78, 4, COLOR_BG, COLOR_DANGER);

    textCenter(msg, W / 2, CY0 + 190, 1, COLOR_TEXT);
    drawFooter("OK = Weiter");
}

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _gfx->fillScreen(COLOR_BG);

    // Farbstreifen je nach Status
    uint16_t statusColor;
    if      (daysLeft < 0)           statusColor = COLOR_DANGER;
    else if (daysLeft <= DANGER_DAYS) statusColor = COLOR_DANGER;
    else if (daysLeft <= WARNING_DAYS)statusColor = COLOR_WARN;
    else                              statusColor = COLOR_OK;

    _gfx->fillRect(0, 0, W, 8, statusColor);

    // Header mit Index
    _gfx->fillRect(0, 8, W, HDR - 8, COLOR_HEADER);
    textCenter(String(index + 1) + " / " + String(total), W / 2, HDR / 2 + 4, 2, 0xFFFF, COLOR_HEADER);

    // Produktname (2 Zeilen)
    String n = name;
    int16_t y = CY0 + 16;
    if (n.length() <= 22) {
        textLeft(n, 10, y, 2, COLOR_TEXT);
    } else {
        textLeft(n.substring(0, 22), 10, y,      2, COLOR_TEXT);
        textLeft(n.substring(22, 44), 10, y + 22, 2, COLOR_TEXT);
    }

    // MHD und Menge
    y = CY0 + 82;
    textLeft("MHD:   " + expiry, 10, y,      1, COLOR_SUBTEXT);
    textLeft("Menge: " + String(qty) + " Stk.", 10, y + 18, 1, COLOR_SUBTEXT);

    _gfx->drawFastHLine(10, y + 40, W - 20, COLOR_SURFACE);

    // Grosse Statusanzeige
    textCenter(daysLabel(daysLeft), W / 2, CY0 + 190, 3, statusColor);
    if (daysLeft >= 0) {
        textCenter("verbleibend", W / 2, CY0 + 230, 1, COLOR_SUBTEXT);
    }

    drawFooter("AUF/AB = blättern", "OK = Löschen");
}
