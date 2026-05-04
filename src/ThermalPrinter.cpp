#include "ThermalPrinter.h"
#include <Preferences.h>

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
    printLine("Protokoll: ESC/POS");
    printLine("");
    align(1);
    barcode128("TEST001");
    printLine("TEST001");
    feed(3);
    cut();
}

uint32_t ThermalPrinter::loadBaud() {
    Preferences p; p.begin("printer", true);
    uint32_t b = p.getUInt("baud", 9600);
    p.end(); return b;
}

void ThermalPrinter::saveBaud(uint32_t baud) {
    Preferences p; p.begin("printer", false);
    p.putUInt("baud", baud);
    p.end();
}

// ── ESC/POS Hilfsbefehle ──────────────────────────────────────

void ThermalPrinter::init() {
    _serial.write(0x1B); _serial.write('@');   // ESC @ – Reset
    delay(50);
}

void ThermalPrinter::align(uint8_t a) {
    _serial.write(0x1B); _serial.write('a'); _serial.write(a);
}

void ThermalPrinter::bold(bool on) {
    _serial.write(0x1B); _serial.write('E'); _serial.write(on ? 1 : 0);
}

void ThermalPrinter::doubleHeight(bool on) {
    // ESC ! bit3=double height, bit4=double width
    _serial.write(0x1B); _serial.write('!'); _serial.write(on ? 0x30 : 0x00);
}

void ThermalPrinter::printLine(const String &text) {
    _serial.print(text);
    _serial.write('\n');
}

void ThermalPrinter::feed(uint8_t lines) {
    _serial.write(0x1B); _serial.write('d'); _serial.write(lines);
}

void ThermalPrinter::cut() {
    // GS V B 0 – Teilschnitt mit Feed
    _serial.write(0x1D); _serial.write('V'); _serial.write(66); _serial.write(0);
}

// Code128 via ESC/POS (neue Syntax: GS k 73 n {B data)
void ThermalPrinter::barcode128(const String &data) {
    // Höhe: GS h 64
    _serial.write(0x1D); _serial.write('h'); _serial.write(64);
    // Breite: GS w 2
    _serial.write(0x1D); _serial.write('w'); _serial.write(2);
    // HRI unter dem Barcode: GS H 2
    _serial.write(0x1D); _serial.write('H'); _serial.write(2);
    // Schriftart HRI: GS f 0
    _serial.write(0x1D); _serial.write('f'); _serial.write(0);

    // GS k 73 n {B<data>
    // n = 2 (für "{B") + data.length()
    uint8_t n = 2 + data.length();
    _serial.write(0x1D); _serial.write('k');
    _serial.write(0x49);       // m = 73 = Code128
    _serial.write(n);
    _serial.write(0x7B);       // '{'
    _serial.write(0x42);       // 'B'  → Charset B (alle druckbaren ASCII)
    _serial.print(data);
    delay(n * 5 + 100);        // Drucker braucht Zeit zum Rendern
}

// ── Label ─────────────────────────────────────────────────────

void ThermalPrinter::printLabel(const String &name, const String &labelCode,
                                 const String &storageDate, const String &expiryDate,
                                 int qty) {
    init();

    // ── Barcode (zentriert) ───────────────────────────────────
    align(1);
    barcode128(labelCode);
    _serial.write('\n');

    // ── Produktname (groß, fett) ──────────────────────────────
    bold(true);
    doubleHeight(true);
    // Auf 24 Zeichen begrenzen (58mm ~ 24 Zeichen bei doppelter Breite)
    String n = name.length() > 12 ? name.substring(0, 12) : name;
    printLine(n);
    doubleHeight(false);
    bold(false);

    // ── Datum-Infos (linksbündig, normal) ────────────────────
    align(0);
    _serial.write('\n');
    printLine("Einlag: " + storageDate);
    if (!expiryDate.isEmpty())
        printLine("MHD:    " + expiryDate);
    if (qty > 1)
        printLine("Menge:  " + String(qty) + " Stk.");

    // ── Abschneiden ───────────────────────────────────────────
    feed(3);
    cut();
}
