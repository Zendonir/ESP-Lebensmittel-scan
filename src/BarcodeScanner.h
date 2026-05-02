#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>

class BarcodeScanner {
public:
    BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);

    void begin();                                    // UART-Modus (GM861)
    void beginBLE(const String &targetName = "");    // BLE HID-Modus

    bool   available();
    String getBarcode();
    void   flush();
    void   enable();   // UART: GM861 aktivieren; BLE: no-op
    void   disable();  // UART: GM861 schlafen;   BLE: no-op

    bool isBLEMode()    const { return _bleMode; }
    bool bleConnected() const { return _bleConnected; }

    // Aufgerufen aus NimBLE-Callbacks (public für Lambda-Zugriff)
    void   _onHIDReport(const uint8_t *data, size_t len);
    void   _setConnected(bool v) { _bleConnected = v; }
    void   _startScan();
    String _targetName;

    static bool   loadBTMode();
    static String loadBTName();
    static void   saveScannerConfig(bool btMode, const String &name);

private:
    HardwareSerial   &_serial;
    uint8_t           _rxPin, _txPin;
    uint32_t          _baud;

    // UART-Puffer
    String            _uartBuf;
    bool              _uartReady  = false;

    // BLE-Puffer (mutex-geschützt)
    String            _bleBuf;
    bool              _bleReady     = false;
    bool              _bleMode      = false;
    volatile bool     _bleConnected = false;
    SemaphoreHandle_t _mutex        = nullptr;
};
