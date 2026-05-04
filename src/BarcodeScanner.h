#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/semphr.h>

struct BLEDeviceInfo {
    String name;
    String addr;
    int    rssi;
};

class BarcodeScanner {
public:
    BarcodeScanner(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);

    void begin();                                    // UART-Modus (GM861)
    void beginBLE(const String &targetName = "");    // BLE HID-Modus

    bool   available();
    String getBarcode();
    void   flush();
    void   enable();
    void   disable();

    bool isBLEMode()    const { return _bleMode; }
    bool bleConnected() const { return _bleConnected; }

    // BLE-Geräte-Suche (unabhängig vom Scanner-Modus)
    static void startDiscoveryScan(int durationSec = 6);
    static bool isDiscovering();
    static std::vector<BLEDeviceInfo> getDiscoveredDevices();

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

    String            _uartBuf;
    bool              _uartReady  = false;

    String            _bleBuf;
    bool              _bleReady     = false;
    bool              _bleMode      = false;
    volatile bool     _bleConnected = false;
    SemaphoreHandle_t _mutex        = nullptr;
};
