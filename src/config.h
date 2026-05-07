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
//  Pins aus dem Waveshare-Schaltplan (verifiziert)
// ============================================================
#define LCD_MOSI   1   // IO1  FSPID
#define LCD_SCK    5   // IO5  FSPICLK
// LCD_CS: nicht vorhanden – auf PCB mit GND verbunden → GFX_NOT_DEFINED nutzen
#define LCD_DC     3   // IO3  Data/Command
#define LCD_RST   -1   // EXIO1 (IO-Expander PCA9554 Bit 1) – wird per I2C gesteuert
#define LCD_BL     6   // IO6  Hintergrundbeleuchtung (PWM)

#ifndef DISPLAY_W
  #define DISPLAY_W 320
#endif
#ifndef DISPLAY_H
  #define DISPLAY_H 480
#endif

// ============================================================
//  Touch – FT6336 (I2C, Adresse 0x38)
//  I2C-Bus gemeinsam mit IO-Expander, ES8311, AXP2101, RTC, IMU
// ============================================================
#define TOUCH_SDA   8   // IO8  I2C SDA
#define TOUCH_SCL   7   // IO7  I2C SCL
#define TOUCH_INT  -1   // EXIO2 (IO-Expander) – kein direkter GPIO, Polling verwenden
#define TOUCH_RST  -1

// ============================================================
//  IO-Expander – PCA9554 / TCA9554 (I2C, Adresse 0x20)
//  EXIO0..7 steuern: LCD_RST, TP_INT, SD_CS, RTC_INT,
//                    AXP_IRQ, SYS_OUT, PA_CTRL, CAM_PWDN
// ============================================================
#define IOEXP_ADDR      0x20
#define IOEXP_LCD_RST   1    // EXIO1 – Display-Reset (aktiv LOW)
#define IOEXP_TP_INT    2    // EXIO2 – Touch-Interrupt (aktiv LOW)
#define IOEXP_PA_CTRL   7    // EXIO7 – Lautsprecher-Verstärker ein (HIGH)

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
//  Integrierter Lautsprecher – I2S → ES8311 Codec → NS4168 Amp
//  Pins aus Waveshare-Schaltplan (verifiziert)
// ============================================================
#define I2S_SPK_MCLK  12   // IO12 MCLK  (ES8311 Master Clock)
#define I2S_SPK_BCK   13   // IO13 BCLK  (I2S Bit Clock)
#define I2S_SPK_DOUT  14   // IO14 ASDOUT (I2S Data Out → ES8311)
#define I2S_SPK_WS    15   // IO15 LRCK  (I2S Word Select / LR Clock)

// Kein externer GPIO-Buzzer (Signalton über I2S-Lautsprecher)
#define BUZZER_PIN  -1

// ============================================================
//  MQTT (optional) – wird über Web-Interface konfiguriert
// ============================================================
#define MQTT_CHECK_INTERVAL_MS  3600000UL  // 1 Stunde
