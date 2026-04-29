#pragma once
#include <Arduino.h>

// ESC/POS TTL-Thermodrucker (58mm, Code128, Textlabels)
class ThermalPrinter {
public:
    ThermalPrinter(HardwareSerial &serial, uint8_t txPin, uint8_t rxPin, uint32_t baud);
    void begin();

    // Druckt ein Lager-Etikett:
    //   name        – Produktname (Klarschrift)
    //   labelCode   – generierter Code ("L00001"), wird als Code128 gedruckt
    //   storageDate – Einlagerdatum "DD.MM.YYYY"
    //   expiryDate  – Haltbarkeitsdatum "DD.MM.YYYY" (kann leer sein)
    void printLabel(const String &name, const String &labelCode,
                    const String &storageDate, const String &expiryDate);

private:
    HardwareSerial &_serial;
    uint8_t _txPin, _rxPin;
    uint32_t _baud;

    void init();
    void align(uint8_t a);      // 0=links 1=mitte 2=rechts
    void bold(bool on);
    void doubleHeight(bool on);
    void printLine(const String &text);
    void feed(uint8_t lines);
    void cut();
    void barcode128(const String &data);
};
