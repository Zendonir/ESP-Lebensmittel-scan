#include "ThermalPrinter.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static constexpr uint16_t DOTS_PER_MM = 8;   // 203 DPI = 8 dots/mm
static constexpr uint16_t LINE_DOTS   = 30;   // Standardzeilenhöhe ESC @

ThermalPrinter::ThermalPrinter(HardwareSerial &serial,
                                uint8_t txPin, uint8_t rxPin, uint32_t baud)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _baud(baud) {}

void ThermalPrinter::begin() {
    _baud = loadBaud();
    _scaleFactor = loadScaleFactor();
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(50);
    init();
}

void ThermalPrinter::restart(uint32_t baud) {
    saveBaud(baud);
    _baud = baud;
    _scaleFactor = loadScaleFactor();
    _serial.end();
    delay(10);
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(50);
    init();
}

void ThermalPrinter::reloadCalib() {
    _scaleFactor = loadScaleFactor();
}

void ThermalPrinter::testPrint() {
    uint16_t backfeedMm = loadBackfeedMm();
    if (backfeedMm > 0) backfeedDots(backfeedMm * DOTS_PER_MM);
    init();

    uint16_t labelMm  = loadLabelMm();
    uint16_t gapMm    = loadGapMm();
    int8_t   offsetMm = loadFeedOffsetMm();

    // Obere Rahmenlinie = Etiketten-Oberkante
    align(0);
    printLine("________________________________");  // 32 '_'

    // Info zentriert
    align(1);
    printLine("Etikett: " + String(labelMm) + " mm");
    printLine("Abstand: " + String(gapMm)   + " mm");
    if (offsetMm != 0)
        printLine("Offset:  " + String(offsetMm) + " mm");
    if (backfeedMm > 0)
        printLine("Rücklauf:" + String(backfeedMm) + " mm");
    align(0);

    // Fülle Etikett bis zur unteren Rahmenlinie
    uint16_t labelDots = labelMm * DOTS_PER_MM;
    if (labelDots > _contentDots + LINE_DOTS)
        feedDots(labelDots - _contentDots - LINE_DOTS);

    // Untere Rahmenlinie = Etiketten-Unterkante
    printLine("--------------------------------");  // 32 '-'

    // Vorschub zum nächsten Etikett (inkl. Offset)
    feedToNextLabel();
    if (loadUseCut()) cut();
}

// ── NVS ──────────────────────────────────────────────────────

uint32_t ThermalPrinter::loadBaud() {
    Preferences p; p.begin("printer", false);
    uint32_t v = p.getUInt("baud", 9600); p.end(); return v;
}
void ThermalPrinter::saveBaud(uint32_t baud) {
    Preferences p; p.begin("printer", false);
    p.putUInt("baud", baud); p.end();
}
uint16_t ThermalPrinter::loadLabelMm() {
    Preferences p; p.begin("printer", false);
    uint16_t v = p.getUShort("label_mm", 29); p.end(); return v;
}
void ThermalPrinter::saveLabelMm(uint16_t mm) {
    Preferences p; p.begin("printer", false);
    p.putUShort("label_mm", mm); p.end();
}
uint16_t ThermalPrinter::loadGapMm() {
    Preferences p; p.begin("printer", false);
    uint16_t v = p.getUShort("gap_mm", 6); p.end(); return v;
}
void ThermalPrinter::saveGapMm(uint16_t mm) {
    Preferences p; p.begin("printer", false);
    p.putUShort("gap_mm", mm); p.end();
}
bool ThermalPrinter::loadUseCut() {
    Preferences p; p.begin("printer", false);
    bool v = p.getBool("use_cut", false); p.end(); return v;
}
void ThermalPrinter::saveUseCut(bool cut) {
    Preferences p; p.begin("printer", false);
    p.putBool("use_cut", cut); p.end();
}
uint16_t ThermalPrinter::loadBackfeedMm() {
    Preferences p; p.begin("printer", false);
    uint16_t v = p.getUShort("backfeed_mm", 0); p.end(); return v;
}
void ThermalPrinter::saveBackfeedMm(uint16_t mm) {
    Preferences p; p.begin("printer", false);
    p.putUShort("backfeed_mm", mm); p.end();
}
int8_t ThermalPrinter::loadFeedOffsetMm() {
    Preferences p; p.begin("printer", false);
    int8_t v = (int8_t)p.getChar("feed_off_mm", 0); p.end(); return v;
}
void ThermalPrinter::saveFeedOffsetMm(int8_t mm) {
    Preferences p; p.begin("printer", false);
    p.putChar("feed_off_mm", mm); p.end();
}

// ── Kalibrierung (NVS "calib") ────────────────────────────────

float ThermalPrinter::loadScaleFactor() {
    Preferences p; p.begin("calib", false);
    float v = p.getFloat("scale", 1.0f); p.end(); return v;
}
void ThermalPrinter::saveScaleFactor(float f) {
    Preferences p; p.begin("calib", false);
    p.putFloat("scale", f); p.end();
}
uint16_t ThermalPrinter::loadDeadZoneMm() {
    Preferences p; p.begin("calib", false);
    uint16_t v = p.getUShort("dead_mm", 0); p.end(); return v;
}
void ThermalPrinter::saveDeadZoneMm(uint16_t mm) {
    Preferences p; p.begin("calib", false);
    p.putUShort("dead_mm", mm); p.end();
}

void ThermalPrinter::printCalibMark(const String &label) {
    init();
    align(1);
    bold(true);
    printLine("----[ " + (label.isEmpty() ? String("MARK") : label) + " ]----");
    bold(false);
    // 3 mm Leerraum damit der Strich gut sichtbar ist
    feedDots(3 * DOTS_PER_MM);
    _contentDots = 0;  // kein Label-Feed nötig
}

void ThermalPrinter::printScaleTest(uint16_t targetMm) {
    init();
    align(1);
    bold(true);
    printLine("----[ A ]----");
    bold(false);
    // Zielabstand vorfahren (skaliert)
    feedDots(targetMm * DOTS_PER_MM);
    // zweiten Strich
    bold(true);
    printLine("----[ B ]----");
    bold(false);
    feedDots(5 * DOTS_PER_MM);
    _contentDots = 0;
}

void ThermalPrinter::rawFeedMm(uint16_t mm) {
    // Vorschub für Kalibrierung – kein _contentDots-Tracking, keine Skalierung
    uint16_t rem = mm * DOTS_PER_MM;
    while (rem > 0) {
        uint8_t chunk = (rem > 255) ? 255 : (uint8_t)rem;
        _serial.write(0x1B); _serial.write('J'); _serial.write(chunk);
        rem -= chunk;
        if (rem > 0) delay(20);
    }
    delay(mm * 10 + 50);
}

// ── ESC/POS Hilfsbefehle ──────────────────────────────────────

void ThermalPrinter::init() {
    _serial.write(0x1B); _serial.write('@');
    delay(50);
    _contentDots = 0;
}

void ThermalPrinter::align(uint8_t a) {
    _serial.write(0x1B); _serial.write('a'); _serial.write(a);
}

void ThermalPrinter::bold(bool on) {
    _serial.write(0x1B); _serial.write('E'); _serial.write(on ? 1 : 0);
}

void ThermalPrinter::printLine(const String &text) {
    _serial.print(text);
    _serial.write('\n');
    _contentDots += LINE_DOTS;
}

void ThermalPrinter::feedDots(uint16_t dots) {
    // Skalierungskorrektur anwenden (aus Kalibrierung)
    uint16_t physical = (_scaleFactor == 1.0f) ? dots
                        : (uint16_t)(dots * _scaleFactor + 0.5f);
    uint16_t rem = physical;
    while (rem > 0) {
        uint8_t chunk = (rem > 255) ? 255 : (uint8_t)rem;
        _serial.write(0x1B); _serial.write('J'); _serial.write(chunk);
        rem -= chunk;
        if (rem > 0) delay(20);
    }
    _contentDots += dots;  // logische Dots tracken (unkorrigiert)
}

void ThermalPrinter::backfeedDots(uint16_t dots) {
    // ESC K n – Rücklauf um n Punkte (falls vom Drucker unterstützt)
    uint16_t rem = dots;
    while (rem > 0) {
        uint8_t chunk = (rem > 255) ? 255 : (uint8_t)rem;
        _serial.write(0x1B); _serial.write('K'); _serial.write(chunk);
        rem -= chunk;
        if (rem > 0) delay(20);
    }
    delay(dots / 2 + 50);   // Zeit für Motorbewegung
}

void ThermalPrinter::feedMm(uint16_t mm) {
    feedDots(mm * DOTS_PER_MM);
}

void ThermalPrinter::cut() {
    _serial.write(0x1D); _serial.write('V'); _serial.write(66); _serial.write(0);
}

void ThermalPrinter::qrCode(const String &data, uint8_t moduleSize) {
    // GS ( k – QR Code (ESC/POS standard)
    // 1. Model 2
    const uint8_t model[] = {0x1D,'(','k',4,0, 49,65,50,0};
    _serial.write(model, sizeof(model));

    // 2. Module size (2=klein, 3=mittel, 4=groß)
    const uint8_t msize[] = {0x1D,'(','k',3,0, 49,67, moduleSize};
    _serial.write(msize, sizeof(msize));

    // 3. Error correction L (48=L, 49=M, 50=Q, 51=H)
    const uint8_t eclevel[] = {0x1D,'(','k',3,0, 49,69,48};
    _serial.write(eclevel, sizeof(eclevel));

    // 4. Store data: pL + pH*256 = len + 3
    uint16_t n  = (uint16_t)data.length() + 3;
    uint8_t  pL = n & 0xFF;
    uint8_t  pH = (n >> 8) & 0xFF;
    const uint8_t hdr[] = {0x1D,'(','k', pL, pH, 49,80,48};
    _serial.write(hdr, sizeof(hdr));
    _serial.print(data);

    // 5. Print
    const uint8_t print[] = {0x1D,'(','k',3,0, 49,81,48};
    _serial.write(print, sizeof(print));

    delay((uint16_t)data.length() * 5 + 150);

    // QR version 1 (21 modules) + Quiet Zone (4*2 modules) = 29 modules
    // 29 * moduleSize dots + spacing
    uint16_t qrDots = (uint16_t)(29 * moduleSize) + 8;
    _contentDots += qrDots;
}

void ThermalPrinter::feedToNextLabel() {
    uint16_t labelDots  = loadLabelMm()    * DOTS_PER_MM;
    uint16_t gapDots    = loadGapMm()      * DOTS_PER_MM;
    int16_t  offsetDots = loadFeedOffsetMm() * (int16_t)DOTS_PER_MM;
    int16_t  remaining  = (int16_t)(labelDots + gapDots) - (int16_t)_contentDots + offsetDots;
    uint16_t feed       = (remaining > (int16_t)gapDots) ? (uint16_t)remaining : gapDots;
    Serial.printf("[Printer] contentDots=%d labelMm=%d gapMm=%d offsetMm=%d feedDots=%d\n",
                  _contentDots, loadLabelMm(), loadGapMm(), loadFeedOffsetMm(), feed);
    if (feed > 0) feedDots(feed);
}

// ── Label ─────────────────────────────────────────────────────

void ThermalPrinter::printLabel(const String &name, const String &labelCode,
                                 const String &storageDate, const String &expiryDate,
                                 int qty) {
    uint16_t backfeedMm = loadBackfeedMm();
    if (backfeedMm > 0) {
        Serial.printf("[Printer] Backfeed %d mm (%d dots)\n",
                      backfeedMm, backfeedMm * DOTS_PER_MM);
        backfeedDots(backfeedMm * DOTS_PER_MM);
    }
    init();

    // Load layout from NVS
    Preferences prefs; prefs.begin("llayout", false);
    String layoutJson = prefs.getString("json", "{}");
    prefs.end();

    struct Elem {
        char  id[8];
        float x_mm;   // horizontal start
        float y_mm;   // vertical start (from top of label)
        float size_mm; // QR: physical size; Text: ignored (w_mm)
        float h_mm;    // Text: height → determines font multiplier
        bool  visible;
    };
    Elem elems[5] = {
        {"qr",      23.0f,  2.0f, 11.0f,  0.0f, true},
        {"name",     0.0f, 14.0f,  0.0f,  5.0f, true},
        {"storage",  0.0f, 20.0f,  0.0f,  4.0f, true},
        {"expiry",   0.0f, 25.0f,  0.0f,  4.0f, true},
        {"qty",      0.0f, 30.0f,  0.0f,  4.0f, false},
    };

    JsonDocument doc;
    if (!deserializeJson(doc, layoutJson)) {
        JsonArray arr = doc["elements"];
        if (arr) {
            for (JsonObject jel : arr) {
                const char* id = jel["id"] | "";
                for (auto& e : elems) {
                    if (strncmp(e.id, id, 7) == 0) {
                        e.x_mm    = jel["x_mm"]    | e.x_mm;
                        e.y_mm    = jel["y_mm"]    | e.y_mm;
                        e.visible = jel["visible"] | e.visible;
                        if (strcmp(e.id, "qr") == 0)
                            e.size_mm = jel["size_mm"] | e.size_mm;
                        else
                            e.h_mm = jel["h_mm"] | e.h_mm;
                        break;
                    }
                }
            }
        }
    }

    // Sort by y_mm ascending (thermal printer prints top-to-bottom)
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4 - i; j++)
            if (elems[j].y_mm > elems[j+1].y_mm) { Elem t = elems[j]; elems[j] = elems[j+1]; elems[j+1] = t; }

    // Left-align base setting
    align(0);

    for (auto& el : elems) {
        if (!el.visible) continue;

        uint16_t targetDots = (uint16_t)(el.y_mm * DOTS_PER_MM);
        if (targetDots > _contentDots)
            feedDots(targetDots - _contentDots);

        // Set absolute horizontal position (ESC $ nL nH)
        uint16_t xDots = (uint16_t)(el.x_mm * DOTS_PER_MM);
        _serial.write(0x1B); _serial.write('$');
        _serial.write((uint8_t)(xDots & 0xFF)); _serial.write((uint8_t)(xDots >> 8));

        if (strncmp(el.id, "qr", 7) == 0) {
            // size_mm → module size: 29 modules (incl. quiet zone) × modSize dots = size
            uint8_t modSize = (uint8_t)roundf(el.size_mm * DOTS_PER_MM / 29.0f);
            if (modSize < 1) modSize = 1;
            if (modSize > 5) modSize = 5;
            qrCode(labelCode, modSize);
            _serial.write('\n'); _contentDots += 8;
        } else {
            // h_mm → font height multiplier (1x=3.75mm, 2x=7.5mm, 3x=11.25mm)
            uint8_t fm = (uint8_t)roundf(el.h_mm / 3.75f);
            if (fm < 1) fm = 1;
            if (fm > 3) fm = 3;
            uint8_t gsn = (fm == 1) ? 0x00 : (fm == 2) ? 0x11 : 0x22;
            _serial.write(0x1D); _serial.write('!'); _serial.write(gsn);

            if (strncmp(el.id, "name", 7) == 0) {
                bold(true);
                printLine(name);
                bold(false);
            } else if (strncmp(el.id, "storage", 7) == 0) {
                printLine("Einlag: " + storageDate);
            } else if (strncmp(el.id, "expiry", 7) == 0) {
                if (!expiryDate.isEmpty()) printLine("MHD:    " + expiryDate);
            } else if (strncmp(el.id, "qty", 7) == 0) {
                if (qty > 1) printLine("Menge:  " + String(qty) + " Stk.");
            }
            _contentDots += LINE_DOTS * (fm - 1);
        }
    }

    // Reset font size to normal
    _serial.write(0x1D); _serial.write('!'); _serial.write((uint8_t)0x00);

    feedToNextLabel();
    if (loadUseCut()) cut();
}
