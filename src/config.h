#pragma once

// ============================================================
//  WiFi – BITTE ANPASSEN
// ============================================================
#define WIFI_SSID     "DeinWiFiName"
#define WIFI_PASSWORD "DeinWiFiPasswort"
#define HOSTNAME      "lebensmittel-scanner"

// ============================================================
//  Display – Waveshare ESP32-S3 1.64" AMOLED (RM67162, QSPI)
//  Pins fest auf der Platine verlötet – NICHT ändern
// ============================================================
#define LCD_CS   10
#define LCD_SCK  12
#define LCD_D0   11   // MOSI / SDA
#define LCD_D1   13
#define LCD_D2   14
#define LCD_D3    9
#define LCD_RST  17
#define LCD_TE   18   // Tearing Effect (optional, für tear-free updates)

// Auflösung – via platformio.ini als DISPLAY_W / DISPLAY_H definiert
// Fallback falls ohne Build-Flag kompiliert wird:
#ifndef DISPLAY_W
  #define DISPLAY_W 280
#endif
#ifndef DISPLAY_H
  #define DISPLAY_H 456
#endif

// ============================================================
//  GM861 Barcode-Scanner (Hardware Serial1)
//  Stecker: GND, VCC (3,3V), TX→GPIO1, RX→GPIO2
//  Standard-Baudrate GM861: 9600
// ============================================================
#define BARCODE_RX_PIN   1
#define BARCODE_TX_PIN   2
#define BARCODE_BAUD  9600

// ============================================================
//  Taster (active LOW, interner Pull-up)
//  Externe Taster zwischen GPIO und GND
// ============================================================
#define BTN_UP    3
#define BTN_DOWN  4
#define BTN_OK    5

// ============================================================
//  Open Food Facts API
// ============================================================
#define OFF_HOST        "world.openfoodfacts.org"
#define OFF_USER_AGENT  "ESP-Lebensmittel-Scanner/1.0"

// ============================================================
//  Speicher
// ============================================================
#define INVENTORY_FILE  "/inventory.json"
#define MAX_ITEMS       500   // 12 MB LittleFS → viel Platz

// ============================================================
//  Ablaufwarnungen (Tage)
// ============================================================
#define WARNING_DAYS    7
#define DANGER_DAYS     3

// ============================================================
//  NTP / Zeitzone
// ============================================================
#define NTP_SERVER   "pool.ntp.org"
#define NTP_SERVER2  "europe.pool.ntp.org"
#define TZ_STRING    "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
//  Display-Farben (RGB565)
//  AMOLED: schwarz = Pixel aus = kein Stromverbrauch → konsequent
//  schwarzen Hintergrund nutzen
// ============================================================
#define COLOR_BG        0x0000   // Schwarz (AMOLED-optimal)
#define COLOR_TEXT      0xFFFF   // Weiß
#define COLOR_SUBTEXT   0x8410   // Grau
#define COLOR_OK        0x07E0   // Grün
#define COLOR_WARN      0xFD20   // Orange
#define COLOR_DANGER    0xF800   // Rot
#define COLOR_ACCENT    0x07FF   // Cyan
#define COLOR_HEADER    0x1926   // Dunkelblau
#define COLOR_SELECTED  0x0320   // Dunkelgrün (aktives Feld)
#define COLOR_SURFACE   0x18C3   // Dunkelgrau (Kacheln)
