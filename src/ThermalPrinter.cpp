#include "ThermalPrinter.h"
#include <Preferences.h>

static constexpr uint16_t DOTS_PER_MM = 8;   // 203 DPI = 8 dots/mm
static constexpr uint16_t LINE_DOTS   = 30;   // Standardzeilenhöhe ESC @

ThermalPrinter::ThermalPrinter(HardwareSerial &serial,
                                uint8_t txPin, uint8_t rxPin, uint32_t baud)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _baud(baud) {}

void ThermalPrinter::begin() {
    _baud = loadBaud();
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(50);
    init();
}

void ThermalPrinter::restart(uint32_t baud) {
    saveBaud(baud);
    _baud = baud;
    _serial.end();
    delay(10);
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    delay(50);
    init();
}

void ThermalPrinter::testPrint() {
    init();
    align(1);
    bold(true);
    printLine("*** Testdruck ***");
    bold(false);
    align(0);
    printLine("ESP32 Lebensmittel-Scanner");
    printLine("Baudrate: " + String(_baud));
    printLine("Etikett:  " + String(loadLabelMm()) + " mm");
    printLine("Abstand:  " + String(loadGapMm()) + " mm");
    printLine("Schnitt:  " + String(loadUseCut() ? "ja" : "nein"));
    printLine("");
    align(1);
    qrCode("TEST001");
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

void ThermalPrinter::feedMm(uint16_t mm) {
    uint16_t dots = mm * DOTS_PER_MM;
    while (dots > 0) {
        uint8_t chunk = (dots > 255) ? 255 : (uint8_t)dots;
        _serial.write(0x1B); _serial.write('J'); _serial.write(chunk);
        dots -= chunk;
        if (dots > 0) delay(20);
    }
    _contentDots += mm * DOTS_PER_MM;
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
    uint16_t labelDots = loadLabelMm() * DOTS_PER_MM;
    uint16_t gapDots   = loadGapMm()   * DOTS_PER_MM;
    int16_t  remaining = (int16_t)(labelDots + gapDots) - (int16_t)_contentDots;
    // Mindestens Abstand vorschieben, auch wenn Inhalt das Etikett überläuft
    uint16_t feedDots  = (remaining > (int16_t)gapDots) ? (uint16_t)remaining : gapDots;
    Serial.printf("[Printer] contentDots=%d labelMm=%d gapMm=%d feedDots=%d\n",
                  _contentDots, loadLabelMm(), loadGapMm(), feedDots);
    if (feedDots > 0) feedMm(feedDots / DOTS_PER_MM + 1);
}

// ── Label ─────────────────────────────────────────────────────
//
// Layout (kompakt, passt auf 29mm Etiketten):
//   [QR-Code zentriert]          ~95 dots (moduleSize=3)
//   [Produktname fett zentriert] 30 dots
//   Einlag: TT.MM.JJJJ          30 dots
//   MHD:    TT.MM.JJJJ          30 dots  (nur wenn vorhanden)
//   Menge:  X Stk.               30 dots  (nur wenn > 1)
// Summe ohne MHD/Menge: ~155 dots ≈ 19.4mm → passt auf 29mm

void ThermalPrinter::printLabel(const String &name, const String &labelCode,
                                 const String &storageDate, const String &expiryDate,
                                 int qty) {
    init();

    // QR-Code zentriert
    align(1);
    qrCode(labelCode, 3);
    _serial.write('\n'); _contentDots += 8;

    // Produktname (fett, max. 18 Zeichen bei 58mm)
    bold(true);
    String n = name.length() > 18 ? name.substring(0, 18) : name;
    printLine(n);
    bold(false);

    // Datum + Menge linksbündig
    align(0);
    printLine("Einlag: " + storageDate);
    if (!expiryDate.isEmpty())
        printLine("MHD:    " + expiryDate);
    if (qty > 1)
        printLine("Menge:  " + String(qty) + " Stk.");

    feedToNextLabel();
    if (loadUseCut()) cut();
}
