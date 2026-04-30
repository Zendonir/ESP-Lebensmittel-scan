#pragma once

// ============================================================
//  WiFi – BITTE ANPASSEN (oder leer lassen und per AP konfigurieren)
// ============================================================
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define HOSTNAME      "lebensmittel-scanner"

// ============================================================
//  WiFi Access Point – wenn kein WLAN konfiguriert / Timeout
// ============================================================
#define AP_SSID     "Lebensmittel-Scanner"
#define AP_PASSWORD "12345678"
#define WIFI_CONFIG_FILE "/wifi_config.json"

// ============================================================
//  Display – Waveshare ESP32-S3 1.64" AMOLED (CO5300, QSPI)
//  Pins intern auf der Platine verdrahtet
// ============================================================
#define LCD_CS    9
#define LCD_SCK  10
#define LCD_D0   11
#define LCD_D1   12
#define LCD_D2   13
#define LCD_D3   14
#define LCD_RST  21

#ifndef DISPLAY_W
  #define DISPLAY_W 456
#endif
#ifndef DISPLAY_H
  #define DISPLAY_H 280
#endif

// ============================================================
//  Touch – CST816S (I2C, intern auf der Platine)
// ============================================================
#define TOUCH_SDA  6
#define TOUCH_SCL  7
#define TOUCH_INT  8
#define TOUCH_RST  -1

// ============================================================
//  Optionaler physischer Taster (BOOT-Button)
// ============================================================
#define BTN_BACK   0

// ============================================================
//  GM861 Barcode-Scanner (Hardware Serial1)
//  GM861 TX → GPIO1  |  GM861 RX → GPIO2
// ============================================================
#define BARCODE_RX_PIN   1
#define BARCODE_TX_PIN   2
#define BARCODE_BAUD  9600

// ============================================================
//  RS232/TTL Mini-Thermodrucker (Hardware Serial2)
//  ESP32 GPIO17 → Drucker RX  |  ESP32 GPIO18 → Drucker TX
// ============================================================
#define PRINTER_TX_PIN   17
#define PRINTER_RX_PIN   18
#define PRINTER_BAUD   9600

// ============================================================
//  Stromsparmodus – Inaktivität in ms (10 Minuten)
// ============================================================
#define POWER_SAVE_MS  600000UL

// ============================================================
//  Open Food Facts API
// ============================================================
#define OFF_HOST        "world.openfoodfacts.org"
#define OFF_USER_AGENT  "ESP-Lebensmittel-Scanner/1.0"

// ============================================================
//  Speicher
// ============================================================
#define INVENTORY_FILE       "/inventory.json"
#define CUSTOM_PRODUCTS_FILE "/custom_products.json"
#define CATEGORIES_FILE      "/categories.json"
#define STORAGE_STATS_FILE   "/storage_stats.json"
#define OFF_CACHE_FILE       "/off_cache.json"
#define SHOPPING_LIST_FILE   "/shopping_list.json"
#define MAX_ITEMS            500
#define MAX_CUSTOM_PRODUCTS  100
#define MAX_STATS_ENTRIES    200
#define MAX_CACHE_ENTRIES     50

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
//  Display-Farben (RGB565, AMOLED: schwarz = Pixel aus)
// ============================================================
#define COLOR_BG        0x0000
#define COLOR_TEXT      0xFFFF
#define COLOR_SUBTEXT   0x8410
#define COLOR_OK        0x07E0
#define COLOR_WARN      0xFD20
#define COLOR_DANGER    0xF800
#define COLOR_ACCENT    0x07FF
#define COLOR_HEADER    0x1926
#define COLOR_SELECTED  0x0B60
#define COLOR_SURFACE   0x18C3
#define COLOR_BTN_OK    0x0640
#define COLOR_BTN_BACK  0x2945

// ============================================================
//  KY-006 Passiv-Buzzer
//  S  → GPIO45  |  +  → 3V3  |  -  → GND
//  (GPIO38-41 sind intern für SD-Karte reserviert!)
// ============================================================
#define BUZZER_PIN  45

// ============================================================
//  MQTT (optional) – wird über Web-Interface konfiguriert
// ============================================================
#define MQTT_CHECK_INTERVAL_MS  3600000UL  // 1 Stunde
