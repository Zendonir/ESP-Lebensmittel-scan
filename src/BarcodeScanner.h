#pragma once
#include <Arduino.h>

class BarcodeScanner {
public:
    BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);

    void begin();
    bool available();
    String getBarcode();
    void flush();
    void enable();
    void disable();
    void debugDump();       // alle 2s: zeigt ob Bytes auf RX ankommen

private:
    HardwareSerial &_serial;
    uint8_t _rxPin, _txPin;
    uint32_t _baud;
    String _buffer;
    bool _ready;
};
