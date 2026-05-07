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
//  Display – Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796, SPI)
//  !! Pins aus dem Waveshare-Schaltplan – bitte bei Abweichungen anpassen !!
// ============================================================
#define LCD_MOSI  11   // SPI2 FSPID
#define LCD_SCK   12   // SPI2 FSPICLK
#define LCD_CS    10   // SPI2 FSPICS0
#define LCD_DC     8   // Data/Command
#define LCD_RST    9   // Reset (oder GFX_NOT_DEFINED falls nicht verdrahtet)
#define LCD_BL    46   // Hintergrundbeleuchtung (PWM)

#ifndef DISPLAY_W
  #define DISPLAY_W 320
#endif
#ifndef DISPLAY_H
  #define DISPLAY_H 480
#endif

// ============================================================
//  Touch – FT6336 (I2C, kompatibel mit FT3168, Adresse 0x38)
//  !! Pins aus dem Waveshare-Schaltplan – bitte bei Abweichungen anpassen !!
// ============================================================
#define TOUCH_SDA  39
#define TOUCH_SCL  40
#define TOUCH_INT  38
#define TOUCH_RST  -1

// ============================================================
//  Optionaler physischer Taster (BOOT-Button)
// ============================================================
#define BTN_BACK   0

// ============================================================
//  GM861 Barcode-Scanner (Hardware Serial1, UART1)
//  GM861 TX → GPIO18 (ESP32 RX)  |  GM861 RX ← GPIO17 (ESP32 TX)
// ============================================================
#define BARCODE_RX_PIN  18
#define BARCODE_TX_PIN  17
#define BARCODE_BAUD  9600

// ============================================================
//  RS232/TTL Mini-Thermodrucker (UART0 = Expansion-Connector)
//  Connector-Label "RXD"(GPIO43) = ESP32-TX → Drucker RX
//  Connector-Label "TXD"(GPIO44) = ESP32-RX ← Drucker TX
// ============================================================
#define PRINTER_TX_PIN   43
#define PRINTER_RX_PIN   44
#define PRINTER_BAUD   9600

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
//  Integrierter Lautsprecher – I2S-Ausgang
//  !! GPIO-Pins bitte aus Waveshare-Schaltplan entnehmen und anpassen !!
//  Typisch: BCK=2, WS=4, DOUT=3 – abhängig von Boardrevision
// ============================================================
#define I2S_SPK_BCK   2    // I2S Bit Clock  → Schaltplan prüfen
#define I2S_SPK_WS    4    // I2S Word Select → Schaltplan prüfen
#define I2S_SPK_DOUT  3    // I2S Data Out   → Schaltplan prüfen

// Kein externer GPIO-Buzzer (Signalton über I2S-Lautsprecher)
#define BUZZER_PIN  -1

// ============================================================
//  MQTT (optional) – wird über Web-Interface konfiguriert
// ============================================================
#define MQTT_CHECK_INTERVAL_MS  3600000UL  // 1 Stunde
