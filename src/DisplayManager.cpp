#include "DisplayManager.h"

DisplayManager::DisplayManager() {}

void DisplayManager::begin() {
    _tft.init();
    _tft.setRotation(0); // Portrait: 240x320
    _tft.fillScreen(COLOR_BG);
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
}

void DisplayManager::setBrightness(uint8_t pct) {
#ifdef TFT_BL
    analogWrite(TFT_BL, map(pct, 0, 100, 0, 255));
#endif
}

// ── Private Helfer ────────────────────────────────────────────

void DisplayManager::drawHeader(const String &title, uint16_t color) {
    _tft.fillRect(0, 0, 240, 36, color);
    _tft.setTextColor(TFT_WHITE, color);
    _tft.setTextSize(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(title, 120, 18);
}

void DisplayManager::drawFooter(const String &left, const String &right) {
    _tft.fillRect(0, 296, 240, 24, COLOR_HEADER);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_HEADER);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.drawString(left, 6, 308);
    if (!right.isEmpty()) {
        _tft.setTextDatum(MR_DATUM);
        _tft.drawString(right, 234, 308);
    }
}

void DisplayManager::drawCentered(const String &text, int y, uint8_t size, uint16_t color) {
    _tft.setTextColor(color, COLOR_BG);
    _tft.setTextSize(size);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(text, 120, y);
}

String DisplayManager::daysLeftLabel(int days) {
    if (days < 0) return "ABGELAUFEN!";
    if (days == 0) return "Heute!";
    if (days == 1) return "1 Tag";
    return String(days) + " Tage";
}

// ── Bildschirme ───────────────────────────────────────────────

void DisplayManager::showBooting(const String &msg) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Lebensmittel-Scanner");
    drawCentered("Starte...", 160, 2, COLOR_ACCENT);
    if (!msg.isEmpty()) drawCentered(msg, 200, 1, COLOR_SUBTEXT);
}

void DisplayManager::showWifiConnecting(const String &ssid) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("WLAN Verbindung");
    drawCentered("Verbinde mit:", 120, 1, COLOR_SUBTEXT);
    drawCentered(ssid, 150, 2, COLOR_ACCENT);

    // Animierter Spinner (statischer Aufruf – update durch wiederholten Aufruf)
    static int dot = 0;
    String dots = "";
    for (int i = 0; i < dot % 4; i++) dots += ".";
    drawCentered(dots + "   ", 200, 2, COLOR_TEXT);
    dot++;
}

void DisplayManager::showIdle(int totalItems, int expiringItems, int expiredItems) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Bereit zum Scannen");

    // Statistik-Kacheln
    // Gesamt
    _tft.fillRoundRect(10, 50, 105, 70, 6, COLOR_HEADER);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_HEADER);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("Gesamt", 62, 68);
    _tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
    _tft.setTextSize(3);
    _tft.drawString(String(totalItems), 62, 100);

    // Bald ablaufend
    uint16_t warnColor = expiringItems > 0 ? 0x8400 : COLOR_HEADER; // dunkelorange
    _tft.fillRoundRect(125, 50, 105, 70, 6, warnColor);
    _tft.setTextColor(COLOR_SUBTEXT, warnColor);
    _tft.setTextSize(1);
    _tft.drawString("Bald weg", 177, 68);
    _tft.setTextColor(expiringItems > 0 ? COLOR_WARN : COLOR_TEXT, warnColor);
    _tft.setTextSize(3);
    _tft.drawString(String(expiringItems), 177, 100);

    // Abgelaufen
    uint16_t expColor = expiredItems > 0 ? 0x6000 : COLOR_HEADER; // dunkelrot
    _tft.fillRoundRect(10, 132, 220, 55, 6, expColor);
    _tft.setTextColor(COLOR_SUBTEXT, expColor);
    _tft.setTextSize(1);
    _tft.drawString("Abgelaufen:", 120, 148);
    _tft.setTextColor(expiredItems > 0 ? COLOR_DANGER : COLOR_TEXT, expColor);
    _tft.setTextSize(3);
    _tft.drawString(String(expiredItems), 120, 170);

    // Anleitung
    drawCentered("Barcode scannen zum Hinzufuegen", 220, 1, COLOR_SUBTEXT);
    drawCentered("oder Web-Interface aufrufen", 238, 1, COLOR_SUBTEXT);
    drawFooter("OK = Web-Info", "");
}

void DisplayManager::showScanning() {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Bitte scannen");

    // Barcode-Symbol (manuell gezeichnet)
    int bx = 40, by = 80, bw = 160, bh = 100;
    for (int x = 0; x < bw; x += 7) {
        int w = (x % 14 < 7) ? 4 : 2;
        _tft.fillRect(bx + x, by, w, bh, COLOR_TEXT);
    }
    _tft.fillRect(bx, by + bh + 4, bw, 2, COLOR_TEXT);

    drawCentered("Barcode vor den Scanner halten", 220, 1, COLOR_SUBTEXT);
    drawFooter("ZURUECK = Abbrechen");
}

void DisplayManager::showFetching(const String &barcode) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Suche Produkt...");
    drawCentered("Open Food Facts", 130, 1, COLOR_SUBTEXT);
    drawCentered(barcode, 160, 2, COLOR_ACCENT);

    static int dot = 0;
    String anim = "";
    for (int i = 0; i < 3; i++) anim += (i <= dot % 4) ? "." : " ";
    drawCentered(anim, 210, 3, COLOR_TEXT);
    dot++;
}

void DisplayManager::showProduct(const ProductInfo &info, int stock) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Produkt gefunden");

    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.setTextSize(2);
    _tft.setTextDatum(TL_DATUM);

    // Name umbrechen wenn nötig (max ~18 Zeichen pro Zeile bei Size 2)
    String name = info.name;
    if (name.length() > 18) {
        _tft.drawString(name.substring(0, 18), 8, 48);
        _tft.drawString(name.substring(18, 36), 8, 70);
    } else {
        _tft.drawString(name, 8, 48);
    }

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    if (!info.brand.isEmpty())    _tft.drawString("Marke: " + info.brand,    8, 100);
    if (!info.quantity.isEmpty()) _tft.drawString("Menge: " + info.quantity, 8, 116);
    if (!info.category.isEmpty()) _tft.drawString("Kat.: "  + info.category, 8, 132);

    // Lagerbestand
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    _tft.drawString("Im Lager: " + String(stock) + " Stk.", 8, 158);

    _tft.fillRect(0, 178, 240, 1, COLOR_SUBTEXT);

    // Buttons
    _tft.fillRoundRect(10, 188, 100, 36, 6, COLOR_OK);
    _tft.setTextColor(TFT_BLACK, COLOR_OK);
    _tft.setTextSize(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("OK", 60, 206);

    _tft.fillRoundRect(130, 188, 100, 36, 6, 0x4208);
    _tft.setTextColor(COLOR_TEXT, 0x4208);
    _tft.drawString("Zurueck", 180, 206);

    drawFooter("Barcode: " + info.barcode.substring(0, 13));
}

void DisplayManager::showDateEntry(const DateInput &d, const String &productName) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Haltbarkeitsdatum");

    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    String pn = productName.length() > 28 ? productName.substring(0, 28) : productName;
    _tft.drawString(pn, 120, 50);

    // Felder: Tag | Monat | Jahr
    struct { int val; const char *label; int x; DateField field; } fields[3] = {
        { d.day,   "Tag",   40,  FIELD_DAY   },
        { d.month, "Monat", 120, FIELD_MONTH },
        { d.year,  "Jahr",  200, FIELD_YEAR  },
    };

    for (auto &f : fields) {
        bool active = (d.activeField == f.field);
        uint16_t bg   = active ? COLOR_SELECTED : 0x2104;
        uint16_t fg   = active ? COLOR_TEXT : COLOR_SUBTEXT;

        _tft.fillRoundRect(f.x - 30, 80, 60, 70, 8, bg);

        _tft.setTextSize(1);
        _tft.setTextColor(fg, bg);
        _tft.drawString(f.label, f.x, 92);

        _tft.setTextSize(3);
        _tft.setTextColor(active ? COLOR_OK : COLOR_TEXT, bg);
        char buf[8];
        if (f.field == FIELD_YEAR)
            snprintf(buf, sizeof(buf), "%d", f.val);
        else
            snprintf(buf, sizeof(buf), "%02d", f.val);
        _tft.drawString(buf, f.x, 125);
    }

    // Datumsvorschau
    char preview[12];
    snprintf(preview, sizeof(preview), "%02d.%02d.%04d", d.day, d.month, d.year);
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    _tft.drawString(preview, 120, 175);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    _tft.drawString("AUF/AB = aendern   OK = bestaetigen", 120, 210);

    drawFooter("AUF  AB  OK");
}

void DisplayManager::showSuccess(const String &productName, const String &date) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Hinzugefuegt!", COLOR_OK);

    _tft.setTextSize(4);
    _tft.setTextColor(COLOR_OK, COLOR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("OK!", 120, 120);

    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    String pn = productName.length() > 18 ? productName.substring(0, 18) : productName;
    _tft.drawString(pn, 120, 190);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    _tft.drawString("MHD: " + date, 120, 220);
}

void DisplayManager::showError(const String &msg) {
    _tft.fillScreen(COLOR_BG);
    drawHeader("Fehler", COLOR_DANGER);

    _tft.setTextSize(3);
    _tft.setTextColor(COLOR_DANGER, COLOR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("!", 120, 120);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.drawString(msg, 120, 190);
    drawFooter("OK = Weiter");
}

void DisplayManager::showInventoryItem(int index, int total, const String &name,
                                        const String &expiry, int qty, int daysLeft) {
    _tft.fillScreen(COLOR_BG);
    String hdr = String(index + 1) + "/" + String(total);
    drawHeader(hdr);

    uint16_t statusColor;
    String statusLabel;
    if (daysLeft < 0)            { statusColor = COLOR_DANGER; statusLabel = "ABGELAUFEN"; }
    else if (daysLeft <= DANGER_DAYS) { statusColor = COLOR_DANGER; statusLabel = daysLeftLabel(daysLeft); }
    else if (daysLeft <= WARNING_DAYS){ statusColor = COLOR_WARN;   statusLabel = daysLeftLabel(daysLeft); }
    else                              { statusColor = COLOR_OK;     statusLabel = daysLeftLabel(daysLeft); }

    _tft.fillRect(0, 36, 240, 6, statusColor);

    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.setTextDatum(TL_DATUM);
    String n = name.length() > 18 ? name.substring(0, 18) : name;
    _tft.drawString(n, 8, 56);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_SUBTEXT, COLOR_BG);
    _tft.drawString("MHD: " + expiry, 8, 90);
    _tft.drawString("Menge: " + String(qty) + " Stk.", 8, 108);

    _tft.setTextSize(2);
    _tft.setTextColor(statusColor, COLOR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(statusLabel, 120, 150);

    drawFooter("AUF/AB = blättern", "OK = Löschen");
}
