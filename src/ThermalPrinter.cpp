#include "ThermalPrinter.h"
#include <Preferences.h>

// Dots pro mm bei typischen 58mm-Thermodruckern (203 DPI = 8 dots/mm)
static constexpr uint16_t DOTS_PER_MM = 8;
// Standard-Zeilenhöhe nach ESC @ = 30 dots ≈ 3.75mm
static constexpr uint16_t LINE_DOTS   = 30;

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
    printLine("");
    printLine("ESP32 Lebensmittel-Scanner");
    printLine("Baudrate: " + String(_baud));
    printLine("Etikett:  " + String(loadLabelMm()) + " mm");
    printLine("Abstand:  " + String(loadGapMm()) + " mm");
    printLine("Schnitt:  " + String(loadUseCut() ? "ja" : "nein"));
    printLine("");
    align(1);
    barcode128("TEST001");
    feedToNextLabel();
    if (loadUseCut()) cut();
}

// ── NVS ──────────────────────────────────────────────────────

uint32_t ThermalPrinter::loadBaud() {
    Preferences p; p.begin("printer", true);
    uint32_t v = p.getUInt("baud", 9600); p.end(); return v;
}
void ThermalPrinter::saveBaud(uint32_t baud) {
    Preferences p; p.begin("printer", false);
    p.putUInt("baud", baud); p.end();
}
uint16_t ThermalPrinter::loadLabelMm() {
    Preferences p; p.begin("printer", true);
    uint16_t v = p.getUShort("label_mm", 40); p.end(); return v;
}
void ThermalPrinter::saveLabelMm(uint16_t mm) {
    Preferences p; p.begin("printer", false);
    p.putUShort("label_mm", mm); p.end();
}
uint16_t ThermalPrinter::loadGapMm() {
    Preferences p; p.begin("printer", true);
    uint16_t v = p.getUShort("gap_mm", 3); p.end(); return v;
}
void ThermalPrinter::saveGapMm(uint16_t mm) {
    Preferences p; p.begin("printer", false);
    p.putUShort("gap_mm", mm); p.end();
}
bool ThermalPrinter::loadUseCut() {
    Preferences p; p.begin("printer", true);
    bool v = p.getBool("use_cut", true); p.end(); return v;
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
    _doubleH = false;
}

void ThermalPrinter::align(uint8_t a) {
    _serial.write(0x1B); _serial.write('a'); _serial.write(a);
}

void ThermalPrinter::bold(bool on) {
    _serial.write(0x1B); _serial.write('E'); _serial.write(on ? 1 : 0);
}

void ThermalPrinter::doubleHeight(bool on) {
    _serial.write(0x1B); _serial.write('!'); _serial.write(on ? 0x30 : 0x00);
    _doubleH = on;
}

void ThermalPrinter::printLine(const String &text) {
    _serial.print(text);
    _serial.write('\n');
    _contentDots += _doubleH ? LINE_DOTS * 2 : LINE_DOTS;
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

void ThermalPrinter::barcode128(const String &data) {
    _serial.write(0x1D); _serial.write('h'); _serial.write(64);
    _serial.write(0x1D); _serial.write('w'); _serial.write(2);
    _serial.write(0x1D); _serial.write('H'); _serial.write(2);
    _serial.write(0x1D); _serial.write('f'); _serial.write(0);

    uint8_t n = 2 + data.length();
    _serial.write(0x1D); _serial.write('k');
    _serial.write(0x49);
    _serial.write(n);
    _serial.write(0x7B);
    _serial.write(0x42);
    _serial.print(data);
    delay(n * 5 + 100);

    // barcode (64) + HRI text (~24) + implizite Zeile (30) ≈ 118 dots
    _contentDots += 118;
}

void ThermalPrinter::feedToNextLabel() {
    uint16_t labelDots = loadLabelMm() * DOTS_PER_MM;
    uint16_t gapDots   = loadGapMm()   * DOTS_PER_MM;
    uint16_t totalPitch = labelDots + gapDots;

    // Verbleibende Dots bis Ende des Etiketts + ganzer Abstand
    int16_t remaining = (int16_t)totalPitch - (int16_t)_contentDots;
    if (remaining > 0) feedMm(remaining / DOTS_PER_MM + 1);
}

// ── Label ─────────────────────────────────────────────────────

void ThermalPrinter::printLabel(const String &name, const String &labelCode,
                                 const String &storageDate, const String &expiryDate,
                                 int qty) {
    init();

    align(1);
    barcode128(labelCode);
    _serial.write('\n'); _contentDots += LINE_DOTS;

    bold(true);
    doubleHeight(true);
    String n = name.length() > 12 ? name.substring(0, 12) : name;
    printLine(n);
    doubleHeight(false);
    bold(false);

    align(0);
    _serial.write('\n'); _contentDots += LINE_DOTS;
    printLine("Einlag: " + storageDate);
    if (!expiryDate.isEmpty())
        printLine("MHD:    " + expiryDate);
    if (qty > 1)
        printLine("Menge:  " + String(qty) + " Stk.");

    feedToNextLabel();
    if (loadUseCut()) cut();
}
