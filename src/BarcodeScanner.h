#pragma once
#include <Arduino.h>
#include <BluetoothSerial.h>

class BarcodeScanner {
public:
    BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);

    void begin();                                               // UART-Modus (eingebauter Scanner)
    void beginBT(const String &deviceName = "Lager-Scanner");  // Bluetooth SPP-Modus

    bool   available();
    String getBarcode();
    void   flush();
    void   enable();   // UART: GM861 aktivieren; BT: no-op
    void   disable();  // UART: GM861 schlafen;   BT: no-op

    bool isBTMode()    const { return _btMode; }
    bool btConnected() const { return _btMode && _bt.connected(); }

    static bool   loadBTMode();
    static String loadBTName();
    static void   saveScannerConfig(bool btMode, const String &btName);

private:
    HardwareSerial &_serial;
    uint8_t  _rxPin, _txPin;
    uint32_t _baud;
    String   _buffer;
    bool     _ready  = false;
    bool     _btMode = false;
    BluetoothSerial _bt;
    Stream  *_stream = nullptr;  // zeigt auf _serial oder _bt
};
