#pragma once
#include <Arduino.h>

class BarcodeScanner {
public:
    BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);

    void begin();
    bool available();       // true wenn ein vollständiger Barcode vorliegt
    String getBarcode();    // gibt den Barcode zurück und leert den Buffer
    void flush();

private:
    HardwareSerial &_serial;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    String _buffer;
    bool _ready;
};
