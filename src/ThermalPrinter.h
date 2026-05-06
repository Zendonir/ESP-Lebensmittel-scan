#pragma once
#include <Arduino.h>

// ESC/POS TTL-Thermodrucker (58mm, Code128, Textlabels)
class ThermalPrinter {
public:
    ThermalPrinter(HardwareSerial &serial, uint8_t txPin, uint8_t rxPin, uint32_t baud);
    void begin();
    void restart(uint32_t baud);
    void testPrint();

    // Drucker-Konfiguration (NVS "printer")
    static uint32_t loadBaud();
    static void     saveBaud(uint32_t baud);
    static float    loadLabelMm();
    static void     saveLabelMm(float mm);
    static float    loadGapMm();       // "Nachlauf" nach Etikett für Abreiß-Puffer
    static void     saveGapMm(float mm);
    static bool     loadUseCut();
    static void     saveUseCut(bool cut);
    static float    loadBackfeedMm();
    static void     saveBackfeedMm(float mm);
    static float    loadFeedOffsetMm();
    static void     saveFeedOffsetMm(float mm);

    // Kalibrierung (NVS "calib")
    static float    loadScaleFactor();
    static void     saveScaleFactor(float f);
    static uint16_t loadDeadZoneMm();
    static void     saveDeadZoneMm(uint16_t mm);
    void            reloadCalib();  // neu aus NVS laden (nach Speichern)

    // Kalibrierdrucke
    void printCalibMark(const String &label = "");
    void printScaleTest(uint16_t targetMm);
    void rawFeedMm(uint16_t mm);   // Papiervorschub ohne Content-Tracking

    void printLabel(const String &name, const String &labelCode,
                    const String &storageDate, const String &expiryDate,
                    int qty = 1);

private:
    HardwareSerial &_serial;
    uint8_t  _txPin, _rxPin;
    uint32_t _baud;
    uint16_t _contentDots = 0;
    float    _scaleFactor = 1.0f;  // gecacht aus NVS

    void init();
    void align(uint8_t a);
    void bold(bool on);
    void printLine(const String &text);
    void feedMm(uint16_t mm);
    void feedDots(uint16_t dots);
    void backfeedDots(uint16_t dots);
    void cut();
    void qrCode(const String &data, uint8_t moduleSize = 3);
    void feedToNextLabel();
};
