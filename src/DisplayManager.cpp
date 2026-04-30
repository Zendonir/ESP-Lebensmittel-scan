#include "DisplayManager.h"
#include "CategoryManager.h"

static constexpr int16_t W   = DISPLAY_W;   // 456
static constexpr int16_t H   = DISPLAY_H;   // 280

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
    _gfx->fillScreen(0x0000);
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
    _gfx->fillRect(0, 0, W, 40, bg);
    textCenter(title, W / 2, 20, 2, 0xFFFF, bg);
}

void DisplayManager::drawTouchButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                      const String &label, uint16_t bg, uint16_t fg,
                                      uint8_t sz) {
    _gfx->fillRoundRect(x, y, w, h, 10, bg);
    textCenter(label, x + w / 2, y + h / 2, sz, fg, bg);
}

void DisplayManager::drawPlusMinusColumn(int col, int value, bool isYear) {
    int16_t cx   = col * 152 + 76;
    int16_t colX = col * 152 + 3;
    int16_t colW = 152 - 6;

    _gfx->fillRoundRect(colX, 58, colW, 56, 8, 0x18C3);
    textCenter("+", cx, 86, 4, 0x07E0, 0x18C3);

    _gfx->fillRect(colX, 116, colW, 74, 0x0000);
    char buf[6];
    if (isYear) snprintf(buf, sizeof(buf), "%d",  value);
    else        snprintf(buf, sizeof(buf), "%02d", value);
    textCenter(buf, cx, 153, 4, 0xFFFF, 0x0000);

    _gfx->fillRoundRect(colX, 192, colW, 46, 8, 0x18C3);
    textCenter("-", cx, 215, 4, 0xF800, 0x18C3);

    if (col < 2)
        _gfx->drawFastVLine((col + 1) * 152, 58, 238 - 58, 0x18C3);
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

void DisplayManager::showMain(int catIndex,
                               const std::vector<CustomProduct> &products,
                               int offset,
                               const std::vector<int> &catInvCounts,
                               int warnCount,
                               bool wifiOk) {
    _gfx->fillScreen(0x0000);
    Serial.printf("[Disp] showMain cats=%d products=%d\n", (int)g_categories.size(), (int)products.size());

    // ── Kategorie-Tabs (dynamische Breite) ───────────────────
    int nCats    = max(1, (int)g_categories.size());
    int16_t tabW = W / nCats;
    for (int i = 0; i < nCats; i++) {
        int16_t tx = i * tabW;
        bool sel = (i == catIndex);
        bool hasCat = (i < (int)g_categories.size());
        uint16_t bg = hasCat ? (sel ? g_categories[i].bgColor  : 0x18C3) : (sel ? 0x07FF : 0x18C3);
        uint16_t fg = hasCat ? (sel ? g_categories[i].textColor : 0x8410) : (sel ? 0x0000    : 0x8410);
        _gfx->fillRect(tx, 0, tabW, 34, bg);

        String abbrev = hasCat ? g_categories[i].name : "Alle";
        if (abbrev.length() > 4) abbrev = abbrev.substring(0, 4) + ".";
        textCenter(abbrev, tx + tabW / 2, 10, 1, fg, bg);

        int cnt = (i < (int)catInvCounts.size()) ? catInvCounts[i] : 0;
        if (cnt > 0) {
            String cntStr = cnt > 99 ? "99+" : String(cnt);
            textCenter(cntStr, tx + tabW / 2, 24, 1, sel ? fg : 0xFFFF, bg);
        }

        if (i > 0) _gfx->drawFastVLine(tx, 0, 34, 0x0000);
    }

    uint16_t wifiDot = wifiOk ? 0x07E0 : 0xF800;
    _gfx->fillCircle(W - 5, 5, 4, wifiDot);

    // ── Produktliste ──────────────────────────────────────────
    if (products.empty()) {
        textCenter("Keine Produkte",               W / 2, 124,  1, 0x8410);
        textCenter("Im Web-Interface hinzufuegen", W / 2, 148, 1, 0x8410);
    } else {
        int visible = min(5, (int)products.size() - offset);
        for (int i = 0; i < visible; i++) {
            const auto &p  = products[offset + i];
            int16_t itemY  = 34 + i * 44;
            uint16_t rowBg = (i % 2 == 0) ? 0x0000 : 0x18C3;
            _gfx->fillRect(0, itemY, W, 44, rowBg);
            String name  = p.name.length()  > 26 ? p.name.substring(0, 26)  : p.name;
            String brand = p.brand.length() > 40 ? p.brand.substring(0, 40) : p.brand;
            textLeft(name, 10, itemY + 4, 2, 0xFFFF, rowBg);
            if (!p.brand.isEmpty())
                textLeft(brand, 10, itemY + 28, 1, 0x8410, rowBg);
            if (p.defaultDays > 0) {
                String d = String(p.defaultDays) + "d";
                textLeft(d, W - 8 - textWidth(d, 1) - 6, itemY + 6, 1, 0x07FF, rowBg);
            }
            _gfx->drawFastHLine(0, itemY + 43, W, 0x18C3);
        }
        if ((int)products.size() > 5) {
            int total  = products.size();
            int barH   = 5 * 44;
            int markH  = max(8, barH * 5 / total);
            int markY  = 34 + (barH - markH) * offset / max(1, total - 5);
            uint16_t sc = (catIndex < (int)g_categories.size()) ? g_categories[catIndex].bgColor : 0x07FF;
            _gfx->fillRect(W - 4, 34, 4, barH, 0x18C3);
            _gfx->fillRect(W - 4, markY, 4, markH, sc);
        }
    }

    // ── Statusleiste unten ───────────────────────────────────
    _gfx->fillRect(0, 254, W, H - 254, 0x18C3);
    textCenter("Barcode scannen oder Produkt antippen",
               W / 2 - 56, 267, 1, 0x8410, 0x18C3);

    int totalInv = 0;
    for (int c : catInvCounts) totalInv += c;
    uint16_t lagerBg = (warnCount > 0) ? 0xFD20 : 0x2945;
    uint16_t lagerFg = (warnCount > 0) ? 0x0000   : 0xFFFF;
    _gfx->fillRoundRect(344, 257, 108, 20, 6, lagerBg);
    String lagerLabel = totalInv > 0 ? "Lager " + String(totalInv) : "Lager";
    textCenter(lagerLabel, 344 + 54, 267, 1, lagerFg, lagerBg);

    _gfx->flush();
}

void DisplayManager::showBooting(const String &msg) {
    _gfx->fillScreen(0x0000);
    textCenter("Lebensmittel", W / 2, H / 2 - 22, 2, 0x07FF);
    textCenter("Scanner",      W / 2, H / 2 +  4, 2, 0x07FF);
    _gfx->drawFastHLine(80, H / 2 + 22, W - 160, 0x18C3);
    if (!msg.isEmpty())
        textCenter(msg, W / 2, H / 2 + 42, 1, 0x8410);
    _gfx->flush();
}

void DisplayManager::showWifiConnecting(const String &ssid, int attempt) {
    _gfx->fillScreen(0x0000);
    drawHeader("WLAN Verbindung");
    textCenter("Verbinde mit:", W / 2,  76, 1, 0x8410);
    textCenter(ssid,            W / 2, 108, 2, 0x07FF);
    int barW   = W - 80;
    int filled = min(barW, (attempt % 20) * (barW / 20));
    _gfx->drawRect(40, 136, barW, 10, 0x18C3);
    if (filled > 0) _gfx->fillRect(40, 136, filled, 10, 0x07FF);
    textCenter("Bitte warten...", W / 2, 164, 1, 0x8410);
    _gfx->flush();
}

void DisplayManager::showScanning() {
    _gfx->fillScreen(0x0000);
    drawHeader("Bitte scannen");
    int bx = 20, by = 48, bw = W - 40, bh = 106;
    for (int x = 0; x < bw; x += 7)
        _gfx->fillRect(bx + x, by, (x % 14 < 7) ? 4 : 2, bh, 0xFFFF);
    _gfx->fillRect(bx, by + bh + 4, bw, 2, 0x8410);
    textCenter("GM861 vor den Scanner halten",   W / 2, 172, 1, 0x8410);
    textCenter("EAN13  EAN8  QR  DataMatrix", W / 2, 192, 1, 0x8410);
    drawTouchButton(8, 228, 440, 44, "Abbrechen", 0x18C3, 0xFFFF);
    _gfx->flush();
}

void DisplayManager::showFetching(const String &barcode) {
    _gfx->fillScreen(0x0000);
    drawHeader("Suche Produkt...");
    textCenter("Open Food Facts", W / 2,  80, 1, 0x8410);
    String b = barcode.length() > 24 ? barcode.substring(0, 24) : barcode;
    textCenter(b, W / 2, 116, 2, 0x07FF);
    static uint8_t dots = 0;
    String d = ""; for (int i = 0; i < (dots % 4); i++) d += " .";
    _gfx->fillRect(0, 144, W, 30, 0x0000);
    textCenter(d, W / 2, 156, 2, 0xFFFF); dots++;
    _gfx->flush();
}

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _gfx->fillScreen(0x0000);
    _gfx->fillRect(0, 0, W, 54, 0x1926);
    textCenter("Haltbarkeitsdatum", W / 2, 14, 1, 0x8410, 0x1926);
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn, W / 2, 40, 2, 0xFFFF, 0x1926);

    const char *labels[] = { "Tag", "Mon", "Jahr" };
    for (int i = 0; i < 3; i++)
        textCenter(labels[i], i * 152 + 76, 58, 1, 0x8410);

    drawPlusMinusColumn(0, d.day,   false);
    drawPlusMinusColumn(1, d.month, false);
    drawPlusMinusColumn(2, d.year,  true);

    drawTouchButton(8, 242, 140, 34, "Abbrechen", 0x18C3, 0x8410, 1);
    drawTouchButton(156, 242, 292, 34, "Bestaetigen", 0x0640, 0xFFFF, 2);
    _gfx->flush();
}

void DisplayManager::showPrinting() {
    _gfx->fillScreen(0x0000);
    drawHeader("Drucke Etikett...", 0x07FF);
    int16_t px = W / 2 - 36, py = 70;
    _gfx->fillRoundRect(px, py,      72, 50, 6, 0x18C3);
    _gfx->fillRect     (px + 8, py - 20, 56, 26, 0x8410);
    _gfx->fillRect     (px + 14, py + 26, 44, 32, 0xFFFF);
    for (int l = 0; l < 3; l++)
        _gfx->fillRect(px + 18, py + 30 + l * 9, 36, 4, 0x18C3);
    textCenter("Bitte warten...", W / 2, 158, 1, 0x8410);
    _gfx->flush();
}

void DisplayManager::showSuccess(const String &productName, const String &date, bool showReprint) {
    _gfx->fillScreen(0x0000);
    drawHeader("Eingelagert!", 0x0640);
    _gfx->fillCircle(W / 2, 116, 44, 0x07E0);
    for (int t = 0; t < 5; t++) {
        _gfx->drawLine(W/2-24+t, 116, W/2-4+t,  137, 0x0000);
        _gfx->drawLine(W/2-4+t,  137, W/2+24+t,  97, 0x0000);
    }
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    textCenter(pn,           W / 2, 178, 2, 0xFFFF);
    textCenter("MHD: "+date, W / 2, 202, 1, 0x8410);
    if (showReprint) {
        drawTouchButton(8,            228, 216, 44, "Nochmal", 0x2945, 0xFFFF, 1);
        drawTouchButton(232, 228, 216, 44, "Weiter",  0x0640,   0xFFFF);
    }
    _gfx->flush();
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(0x0000);
    drawHeader("Fehler", 0xF800);
    _gfx->fillCircle(W / 2, 128, 46, 0xF800);
    textCenter("!", W / 2, 116, 4, 0x0000, 0xF800);
    textCenter(msg, W / 2, 196, 1, 0xFFFF);
    drawTouchButton(8, 228, 440, 44, "OK", 0x2945, 0xFFFF);
    _gfx->flush();
}

void DisplayManager::showRetrieve(const String &name, const String &storageDate,
                                   const String &expiryDate, int daysLeft) {
    _gfx->fillScreen(0x0000);
    drawHeader("Auslagerung", 0x07FF);
    String n = name.length() > 28 ? name.substring(0, 28) : name;
    textCenter(n, W / 2, 72, 2, 0xFFFF);
    textCenter("Eingelagert: " + storageDate, W / 2, 106, 1, 0x8410);
    textCenter("MHD: " + expiryDate,          W / 2, 126, 1, 0x8410);
    uint16_t sc = (daysLeft < 0)             ? 0xF800
                : (daysLeft <= 3)            ? 0xF800
                : (daysLeft <= 7)            ? 0xFD20
                :                              0x07E0;
    textCenter(daysLabel(daysLeft), W / 2, 164, 3, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W / 2, 196, 1, 0x8410);
    drawTouchButton(8, 176, 440, 44, "Behalten", 0x18C3, 0x8410, 1);
    drawTouchButton(8, 228, 440, 44, "Auslagern", 0x0640, 0xFFFF);
    _gfx->flush();
}

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _gfx->fillScreen(0x0000);
    uint16_t sc = (daysLeft < 0 || daysLeft <= 3) ? 0xF800
                : (daysLeft <= 7)                   ? 0xFD20
                :                                     0x07E0;
    _gfx->fillRect(0, 0, W, 6, sc);
    _gfx->fillRect(0, 6, W, 34, 0x1926);
    textCenter(String(index + 1) + " / " + String(total), W / 2, 23, 2, 0xFFFF, 0x1926);

    int16_t y = 48;
    String n = name.length() > 26 ? name.substring(0, 26) : name;
    textLeft(n, 10, y, 2, 0xFFFF); y += 28;
    if (name.length() > 26) {
        textLeft(name.substring(26, 52), 10, y, 2, 0xFFFF); y += 28;
    }
    y += 6;
    textLeft("MHD:   " + expiry,                10, y,      1, 0x8410);
    textLeft("Menge: " + String(qty) + " Stk.", 10, y + 22, 1, 0x8410);

    textCenter(daysLabel(daysLeft), W * 3 / 4, 122, 3, sc);
    if (daysLeft >= 0) textCenter("verbleibend", W * 3 / 4, 154, 1, 0x8410);

    textCenter("< wischen >", W / 2, 162, 1, 0x18C3);
    drawTouchButton(8, 176, 440, 44, "Zurueck",          0x18C3, 0x8410, 1);
    drawTouchButton(8, 228, 440, 44, "Artikel loeschen", 0xF800,  0xFFFF);
    _gfx->flush();
}

void DisplayManager::showAPMode(const String &ssid, const String &password, const String &ip) {
    _gfx->fillScreen(0x0000);
    drawHeader("WiFi Einrichtung", 0xFD20);
    textCenter("Kein WLAN konfiguriert!",    W / 2,  60, 1, 0xFD20);
    textCenter("Netz:",                      W / 2,  80, 1, 0x8410);
    textCenter(ssid,                         W / 2, 106, 2, 0x07FF);
    textCenter("PW:",                        W / 2, 128, 1, 0x8410);
    textCenter(password,                     W / 2, 152, 2, 0xFFFF);
    _gfx->drawFastHLine(20, 170, W - 40, 0x18C3);
    textCenter("Browser: http://" + ip,      W / 2, 188, 1, 0x07FF);
    drawTouchButton(8, 172, 440, 40, "Zur Produktliste", 0x0640, 0xFFFF);
    drawTouchButton(8, 220, 440, 40, "Inventar anzeigen", 0x2945, 0xFFFF);
    _gfx->flush();
}
