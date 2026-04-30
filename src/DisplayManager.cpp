#include "DisplayManager.h"
#include "CategoryManager.h"

static constexpr int16_t W   = DISPLAY_W;   // 456
static constexpr int16_t H   = DISPLAY_H;   // 280
static constexpr int16_t HDR = 40;

// ── Init ──────────────────────────────────────────────────────

DisplayManager::DisplayManager() : _bus(nullptr), _panel(nullptr), _gfx(nullptr) {}

bool DisplayManager::begin() {
    Serial.printf("[Disp] CS=%d SCK=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                  LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_RST);
    _bus   = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    _panel = new Arduino_CO5300(_bus, LCD_RST, 0, W, H, 0, 0, 0, 0);
    _gfx   = new Arduino_Canvas(W, H, _panel);
    Serial.printf("[Disp] canvas ptr=%p\n", (void*)_gfx);
    bool ok = _gfx->begin();
    Serial.printf("[Disp] gfx->begin()=%d  psram free after=%u\n", ok, ESP.getFreePsram());
    if (!ok) return false;
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

void DisplayManager::drawPlusMinusColumn(int col, int value, bool isYear) {
    int16_t cx   = col * DATE_COL_W + DATE_COL_W / 2;
    int16_t colX = col * DATE_COL_W + 3;
    int16_t colW = DATE_COL_W - 6;

    _gfx->fillRoundRect(colX, DATE_PLUS_Y0, colW, DATE_PLUS_Y1 - DATE_PLUS_Y0, 8, COLOR_SURFACE);
    textCenter("+", cx, DATE_PLUS_Y0 + (DATE_PLUS_Y1 - DATE_PLUS_Y0) / 2, 4, COLOR_OK, COLOR_SURFACE);

    _gfx->fillRect(colX, DATE_VAL_Y0, colW, DATE_VAL_Y1 - DATE_VAL_Y0, COLOR_BG);
    char buf[6];
    if (isYear) snprintf(buf, sizeof(buf), "%d",  value);
    else        snprintf(buf, sizeof(buf), "%02d", value);
    textCenter(buf, cx, DATE_VAL_Y0 + (DATE_VAL_Y1 - DATE_VAL_Y0) / 2, 4, COLOR_TEXT, COLOR_BG);

    _gfx->fillRoundRect(colX, DATE_MINUS_Y0, colW, DATE_MINUS_Y1 - DATE_MINUS_Y0, 8, COLOR_SURFACE);
    textCenter("-", cx, DATE_MINUS_Y0 + (DATE_MINUS_Y1 - DATE_MINUS_Y0) / 2, 4, COLOR_DANGER, COLOR_SURFACE);

    if (col < 2)
        _gfx->drawFastVLine((col + 1) * DATE_COL_W, DATE_PLUS_Y0,
                             DATE_MINUS_Y1 - DATE_PLUS_Y0, COLOR_SURFACE);
}

String DisplayManager::daysLabel(int days) {
    if (days < 0)  return "ABGELAUFEN";
    if (days == 0) return "Heute!";
    if (days == 1) return "1 Tag";
    return String(days) + " Tage";
}

// ── Kategorie-Icons ───────────────────────────────────────────

void DisplayManager::drawCategoryIcon(uint8_t cat, int16_t cx, int16_t cy) {
    switch (cat) {
    case 0:
        _gfx->fillRoundRect(cx-19,cy-9,38,20,6,0xD00A);
        _gfx->fillRect(cx+13,cy-18,5,36,0xEF5B);
        _gfx->fillCircle(cx+15,cy-20,6,0xEF5B);
        _gfx->fillCircle(cx+15,cy+20,6,0xEF5B);
        break;
    case 1:
        _gfx->fillCircle(cx-2,cy-9,17,0xD580);
        _gfx->fillRoundRect(cx-4,cy+8,8,18,4,0xEF7B);
        _gfx->fillCircle(cx,cy+27,6,0xD580);
        break;
    case 2:
        _gfx->fillEllipse(cx-4,cy,20,12,0x07DF);
        _gfx->fillTriangle(cx+15,cy-12,cx+15,cy+12,cx+27,cy,0x07DF);
        _gfx->fillCircle(cx-13,cy-4,3,0xFFFF);
        _gfx->fillTriangle(cx-6,cy-12,cx+4,cy-12,cx-1,cy-19,0x07BF);
        break;
    case 3:
        _gfx->fillCircle(cx-8,cy-6,11,0x07E0);
        _gfx->fillCircle(cx+8,cy-6,11,0x07E0);
        _gfx->fillCircle(cx,cy-15,11,0x4FE0);
        _gfx->fillRect(cx-3,cy+6,6,14,0x5B00);
        break;
    case 4:
        _gfx->fillCircle(cx-7,cy+5,13,0xF800);
        _gfx->fillCircle(cx+9,cy+1,11,0xFD40);
        _gfx->fillRect(cx-5,cy-17,2,8,0x0460);
        _gfx->fillEllipse(cx-1,cy-16,5,3,0x04A0);
        break;
    case 5:
        _gfx->drawCircle(cx,cy,19,0xFFFF);
        _gfx->drawCircle(cx,cy,13,0xFFFF);
        _gfx->fillRect(cx-11,cy-16,2,30,0xFFFF);
        _gfx->fillRect(cx-13,cy-16,2,10,0xFFFF);
        _gfx->fillRect(cx-9,cy-16,2,10,0xFFFF);
        _gfx->fillRect(cx+9,cy-16,2,30,0xFFFF);
        _gfx->fillRoundRect(cx+9,cy-16,5,14,2,0xD6DB);
        break;
    case 6:
        _gfx->fillRoundRect(cx-19,cy-2,38,16,6,0x8320);
        _gfx->fillCircle(cx-8,cy-5,12,0xA400);
        _gfx->fillCircle(cx+8,cy-5,12,0xA400);
        _gfx->drawLine(cx,cy-15,cx,cy+10,0x6240);
        break;
    case 7: {
        uint16_t sc=0xB7FF;
        for(int t=0;t<=1;t++){
            _gfx->drawLine(cx+t,cy-21,cx+t,cy+21,sc);
            _gfx->drawLine(cx-18+t,cy-11,cx+18+t,cy+11,sc);
            _gfx->drawLine(cx-18+t,cy+11,cx+18+t,cy-11,sc);
        }
        _gfx->drawLine(cx-6,cy-15,cx,cy-21,sc);
        _gfx->drawLine(cx+6,cy-15,cx,cy-21,sc);
        _gfx->drawLine(cx-6,cy+15,cx,cy+21,sc);
        _gfx->drawLine(cx+6,cy+15,cx,cy+21,sc);
        _gfx->fillCircle(cx,cy,4,sc);
        break;
    }
    }
}

// ── Hauptscreen ───────────────────────────────────────────────
// Kategorietabs (oben) + Produktliste der aktiven Kategorie

void DisplayManager::showMain(int catIndex,
                               const std::vector<CustomProduct> &products,
                               int offset,
                               const std::vector<int> &catInvCounts,
                               int warnCount,
                               bool wifiOk) {
    _gfx->fillScreen(COLOR_BG);
    Serial.printf("[Disp] showMain cats=%d products=%d\n", (int)g_categories.size(), (int)products.size());

    // ── Kategorie-Tabs (dynamische Breite) ───────────────────
    int nCats    = max(1, (int)g_categories.size());
    int16_t tabW = W / nCats;
    for (int i = 0; i < nCats; i++) {
        int16_t tx = i * tabW;
        bool sel = (i == catIndex);
        bool hasCat = (i < (int)g_categories.size());
        uint16_t bg = hasCat ? (sel ? g_categories[i].bgColor  : COLOR_SURFACE) : (sel ? COLOR_ACCENT : COLOR_SURFACE);
        uint16_t fg = hasCat ? (sel ? g_categories[i].textColor : COLOR_SUBTEXT) : (sel ? COLOR_BG    : COLOR_SUBTEXT);
        _gfx->fillRect(tx, 0, tabW, TABS_H, bg);

        // Kategorie-Name (oben)
        String abbrev = hasCat ? g_categories[i].name : "Alle";
        if (abbrev.length() > 4) abbrev = abbrev.substring(0, 4) + ".";
        textCenter(abbrev, tx + tabW / 2, 10, 1, fg, bg);

        // Inventar-Zähler (unten, klein)
        int cnt = (i < (int)catInvCounts.size()) ? catInvCounts[i] : 0;
        if (cnt > 0) {
            String cntStr = cnt > 99 ? "99+" : String(cnt);
            textCenter(cntStr, tx + tabW / 2, 24, 1, sel ? fg : COLOR_TEXT, bg);
        }

        if (i > 0) _gfx->drawFastVLine(tx, 0, TABS_H, COLOR_BG);
    }

    // WiFi-Status-Punkt (oben rechts im Tab-Bereich)
    uint16_t wifiDot = wifiOk ? COLOR_OK : COLOR_DANGER;
    _gfx->fillCircle(W - 5, 5, 4, wifiDot);

    // ── Produktliste ──────────────────────────────────────────
    if (products.empty()) {
        textCenter("Keine Produkte",               W / 2, MAIN_LIST_Y + 90,  1, COLOR_SUBTEXT);
        textCenter("Im Web-Interface hinzufuegen", W / 2, MAIN_LIST_Y + 114, 1, COLOR_SUBTEXT);
    } else {
        int visible = min((int)MAIN_MAX_VIS, (int)products.size() - offset);
        for (int i = 0; i < visible; i++) {
            const auto &p  = products[offset + i];
            int16_t itemY  = MAIN_LIST_Y + i * MAIN_ITEM_H;
            uint16_t rowBg = (i % 2 == 0) ? COLOR_BG : COLOR_SURFACE;
            _gfx->fillRect(0, itemY, W, MAIN_ITEM_H, rowBg);
            String name  = p.name.length()  > 26 ? p.name.substring(0, 26)  : p.name;
            String brand = p.brand.length() > 40 ? p.brand.substring(0, 40) : p.brand;
            textLeft(name, 10, itemY + 4, 2, COLOR_TEXT, rowBg);
            if (!p.brand.isEmpty())
                textLeft(brand, 10, itemY + 28, 1, COLOR_SUBTEXT, rowBg);
            // Standard-MHD-Hinweis rechts
            if (p.defaultDays > 0) {
                String d = String(p.defaultDays) + "d";
                textLeft(d, W - 8 - textWidth(d, 1) - 6, itemY + 6, 1, COLOR_ACCENT, rowBg);
            }
            _gfx->drawFastHLine(0, itemY + MAIN_ITEM_H - 1, W, COLOR_SURFACE);
        }
        // Scrollbalken
        if ((int)products.size() > MAIN_MAX_VIS) {
            int total  = products.size();
            int barH   = MAIN_MAX_VIS * MAIN_ITEM_H;
            int markH  = max(8, barH * MAIN_MAX_VIS / total);
            int markY  = MAIN_LIST_Y + (barH - markH) * offset / max(1, total - MAIN_MAX_VIS);
            uint16_t sc = (catIndex < (int)g_categories.size()) ? g_categories[catIndex].bgColor : COLOR_ACCENT;
            _gfx->fillRect(W - 4, MAIN_LIST_Y, 4, barH, COLOR_SURFACE);
            _gfx->fillRect(W - 4, markY, 4, markH, sc);
        }
    }

    // ── Statusleiste unten ───────────────────────────────────
    _gfx->fillRect(0, MAIN_HINT_Y, W, H - MAIN_HINT_Y, COLOR_SURFACE);
    textCenter("Barcode scannen oder Produkt antippen",
               MAIN_INV_X / 2, MAIN_HINT_Y + 13, 1, COLOR_SUBTEXT, COLOR_SURFACE);

    // "Lager"-Button rechts — orange wenn Ablauf-Warnungen vorhanden
    int totalInv = 0;
    for (int c : catInvCounts) totalInv += c;
    uint16_t lagerBg = (warnCount > 0) ? COLOR_WARN : COLOR_BTN_BACK;
    uint16_t lagerFg = (warnCount > 0) ? COLOR_BG   : COLOR_TEXT;
    _gfx->fillRoundRect(MAIN_INV_X, MAIN_HINT_Y + 3, MAIN_INV_W - 4, H - MAIN_HINT_Y - 6,
                        6, lagerBg);
    String lagerLabel = totalInv > 0 ? "Lager " + String(totalInv) : "Lager";
    textCenter(lagerLabel, MAIN_INV_X + (MAIN_INV_W - 4) / 2, MAIN_HINT_Y + 13,
               1, lagerFg, lagerBg);

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

// ── Datumseingabe ─────────────────────────────────────────────

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _gfx->fillScreen(COLOR_BG);
    _gfx->fillRect(0, 0, W, 54, COLOR_HEADER);
    textCenter("Haltbarkeitsdatum", W / 2, 14, 1, COLOR_SUBTEXT, COLOR_HEADER);
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn, W / 2, 40, 2, COLOR_TEXT, COLOR_HEADER);

    const char *labels[] = { "Tag", "Mon", "Jahr" };
    for (int i = 0; i < 3; i++)
        textCenter(labels[i], i * DATE_COL_W + DATE_COL_W / 2, 58, 1, COLOR_SUBTEXT);

    drawPlusMinusColumn(0, d.day,   false);
    drawPlusMinusColumn(1, d.month, false);
    drawPlusMinusColumn(2, d.year,  true);

    drawTouchButton(DATE_BACK_X, DATE_BTN_Y, DATE_BACK_W, DATE_BTN_H,
                    "Abbrechen", COLOR_SURFACE, COLOR_SUBTEXT, 1);
    drawTouchButton(DATE_OK_X, DATE_BTN_Y, DATE_OK_W, DATE_BTN_H,
                    "Bestaetigen", COLOR_BTN_OK, COLOR_TEXT, 2);
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
