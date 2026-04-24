#pragma once

// ============================================================
//  WiFi – BITTE ANPASSEN
// ============================================================
#define WIFI_SSID     "DeinWiFiName"
#define WIFI_PASSWORD "DeinWiFiPasswort"
#define HOSTNAME      "lebensmittel-scanner"

// ============================================================
//  Taster (active LOW, interner Pull-up)
//  GPIO 34/35 sind Input-only an ESP32 DevKit
// ============================================================
#define BTN_UP    34
#define BTN_DOWN  35
#define BTN_OK    32

// ============================================================
//  Barcode-Scanner UART (Hardware Serial2)
//  Gängige Scanner: GM65, DE2120, Datalogic, etc.
// ============================================================
#define BARCODE_RX_PIN  16
#define BARCODE_TX_PIN  17
#define BARCODE_BAUD    9600

// ============================================================
//  Open Food Facts API
// ============================================================
#define OFF_HOST        "world.openfoodfacts.org"
#define OFF_USER_AGENT  "ESP-Lebensmittel-Scanner/1.0 (github.com/zendonir/esp-lebensmittel-scan)"

// ============================================================
//  Speicher
// ============================================================
#define INVENTORY_FILE  "/inventory.json"
#define MAX_ITEMS       200

// ============================================================
//  Ablaufwarnungen (Tage)
// ============================================================
#define WARNING_DAYS    7
#define DANGER_DAYS     3

// ============================================================
//  NTP / Zeitzone
// ============================================================
#define NTP_SERVER      "pool.ntp.org"
#define NTP_SERVER2     "europe.pool.ntp.org"
// POSIX TZ-String für Deutschland (CET/CEST mit Sommerzeit)
#define TZ_STRING       "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
//  Display-Farben (RGB565)
// ============================================================
#define COLOR_BG        TFT_BLACK
#define COLOR_TEXT      TFT_WHITE
#define COLOR_SUBTEXT   0xC618   // Hellgrau
#define COLOR_OK        TFT_GREEN
#define COLOR_WARN      TFT_YELLOW
#define COLOR_DANGER    TFT_RED
#define COLOR_ACCENT    TFT_CYAN
#define COLOR_HEADER    0x1926   // Dunkelblau
#define COLOR_SELECTED  0x4B0D   // Dunkelgrün (ausgewähltes Feld)
