#pragma once
#include <Arduino.h>

// ESC/POS TTL-Thermodrucker (58mm, Code128, Textlabels)
class ThermalPrinter {
public:
    ThermalPrinter(HardwareSerial &serial, uint8_t txPin, uint8_t rxPin, uint32_t baud);
    void begin();
    void restart(uint32_t baud);
    void testPrint();

    static uint32_t loadBaud();
    static void     saveBaud(uint32_t baud);
    static uint16_t loadLabelMm();
    static void     saveLabelMm(uint16_t mm);
    static uint16_t loadGapMm();
    static void     saveGapMm(uint16_t mm);
    static bool     loadUseCut();
    static void     saveUseCut(bool cut);

    void printLabel(const String &name, const String &labelCode,
                    const String &storageDate, const String &expiryDate,
                    int qty = 1);

private:
    HardwareSerial &_serial;
    uint8_t  _txPin, _rxPin;
    uint32_t _baud;
    uint16_t _contentDots = 0;  // dots printed since last init()
    bool     _doubleH     = false;

    void init();
    void align(uint8_t a);
    void bold(bool on);
    void doubleHeight(bool on);
    void printLine(const String &text);
    void feedMm(uint16_t mm);
    void cut();
    void barcode128(const String &data);

    // Vorschub auf nächste Etikettenoberkante
    void feedToNextLabel();
};
