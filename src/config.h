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
//  Touch – FT3168 (I2C, intern auf der Platine)
// ============================================================
#define TOUCH_SDA  47
#define TOUCH_SCL  48
#define TOUCH_INT  -1
#define TOUCH_RST  -1

// ============================================================
//  Optionaler physischer Taster (BOOT-Button)
// ============================================================
#define BTN_BACK   0

// ============================================================
//  GM861 Barcode-Scanner (Hardware Serial)
//  GM861 TX → GPIO44 (RXD)  |  GM861 RX → GPIO43 (TXD)
// ============================================================
#define BARCODE_RX_PIN  44
#define BARCODE_TX_PIN  43
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
//  Ablaufwarnungen (Tage) – aus g_uiCfg
// ============================================================
#define WARNING_DAYS    (g_uiCfg.warning_days)
#define DANGER_DAYS     (g_uiCfg.danger_days)

// ============================================================
//  NTP / Zeitzone
// ============================================================
#define NTP_SERVER   "pool.ntp.org"
#define NTP_SERVER2  "europe.pool.ntp.org"
#define TZ_STRING    "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
//  Display-Farben – aus NVS laden (UIConfig.h)
// ============================================================
#include "UIConfig.h"
#define COLOR_BG        (g_uiCfg.bg)
#define COLOR_TEXT      (g_uiCfg.text)
#define COLOR_SUBTEXT   (g_uiCfg.subtext)
#define COLOR_OK        (g_uiCfg.ok)
#define COLOR_WARN      (g_uiCfg.warn)
#define COLOR_DANGER    (g_uiCfg.danger)
#define COLOR_ACCENT    (g_uiCfg.accent)
#define COLOR_HEADER    (g_uiCfg.header)
#define COLOR_SELECTED  (g_uiCfg.surface)
#define COLOR_SURFACE   (g_uiCfg.surface)
#define COLOR_BTN_OK    (g_uiCfg.btn_ok)
#define COLOR_BTN_BACK  (g_uiCfg.btn_back)

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
