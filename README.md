# ESP32 Lebensmittel-Lagersystem

Barcode-Scanner mit ESP32 der automatisch Produktinformationen aus der Open Food Facts Datenbank lädt und ein lokales Inventar mit Haltbarkeitsdaten verwaltet.

## Funktionen

- Barcode scannen → Produktname automatisch laden (Open Food Facts)
- Haltbarkeitsdatum am Display eingeben (Tag / Monat / Jahr)
- Lokales Inventar auf dem ESP32 gespeichert (LittleFS)
- Web-Interface im Browser: Artikel ansehen, filtern, löschen, CSV exportieren
- Farbliche Warnungen: grün / gelb (≤7 Tage) / rot (≤3 Tage / abgelaufen)
- Inventar-Durchblättern direkt am Gerät

---

## Hardware

### Empfohlene Komponenten

| Teil | Beschreibung |
|------|-------------|
| ESP32 DevKit v1 | Mikrocontroller (240 MHz, WiFi, BT) |
| 2,8" TFT Display ILI9341 | 240×320 px, SPI-Schnittstelle |
| Serieller Barcode-Scanner | z.B. GM65, DE2120 oder ähnlicher UART-Scanner |
| 3× Taster | Normaler Drucktaster, aktiv LOW |

### Verdrahtung

#### Display (ILI9341, SPI)

| Display-Pin | ESP32-Pin |
|-------------|-----------|
| MOSI (SDI)  | GPIO 23   |
| MISO (SDO)  | GPIO 19   |
| CLK (SCK)   | GPIO 18   |
| CS          | GPIO 5    |
| DC / RS     | GPIO 2    |
| RST         | GPIO 4    |
| BL (Backlight) | GPIO 21 |
| VCC         | 3,3 V     |
| GND         | GND       |

#### Barcode-Scanner (UART)

| Scanner-Pin | ESP32-Pin |
|-------------|-----------|
| TX          | GPIO 16 (RX) |
| RX          | GPIO 17 (TX) |
| VCC         | 3,3 V oder 5 V (je Modul) |
| GND         | GND       |

#### Taster

| Taster | ESP32-Pin | Funktion |
|--------|-----------|----------|
| AUF    | GPIO 34   | Wert erhöhen / vorheriger Artikel |
| AB     | GPIO 35   | Wert verringern / nächster Artikel |
| OK     | GPIO 32   | Bestätigen / Löschen |

Alle Taster zwischen GPIO und GND schalten (kein externer Pull-up nötig).

---

## Software einrichten

### 1. PlatformIO installieren

```bash
pip install platformio
# oder VS Code Extension "PlatformIO IDE"
```

### 2. WiFi-Zugangsdaten eintragen

Datei `src/config.h` öffnen und anpassen:

```cpp
#define WIFI_SSID     "DeinWiFiName"
#define WIFI_PASSWORD "DeinWiFiPasswort"
```

### 3. Display anpassen (falls kein ILI9341)

In `platformio.ini` die `build_flags` anpassen:

```ini
; Für ST7789 (240×240):
-DST7789_DRIVER=1
-DTFT_WIDTH=240
-DTFT_HEIGHT=240

; Für SSD1306 OLED: Anderes Bibliotheks-Setup erforderlich
```

Alle unterstützten Treiber: [TFT_eSPI Dokumentation](https://github.com/Bodmer/TFT_eSPI)

### 4. Kompilieren und flashen

```bash
# Firmware flashen
pio run --target upload

# Web-Interface auf LittleFS hochladen
pio run --target uploadfs

# Serielle Ausgabe
pio device monitor
```

### 5. IP-Adresse herausfinden

Nach dem Start zeigt der serielle Monitor:
```
[WiFi] IP: 192.168.1.42
[Web] Server gestartet auf Port 80
```

Browser öffnen: `http://192.168.1.42`

---

## Bedienung am Gerät

### Artikel hinzufügen

1. Barcode vor den Scanner halten
2. Produktname wird automatisch geladen (benötigt WiFi)
3. Mit **AUF / AB** das Haltbarkeitsdatum einstellen
4. Mit **OK** durch die Felder (Tag → Monat → Jahr → Speichern)

### Inventar durchblättern

- Im Idle-Bildschirm **OK** drücken
- **AUF / AB** zum Blättern
- **OK** drückt löscht den angezeigten Artikel
- **AUF lang halten** (1,5 s) → zurück zum Startbildschirm

### Farb-Bedeutung

| Farbe | Bedeutung |
|-------|-----------|
| Grün  | Haltbarkeit > 7 Tage |
| Gelb  | ≤ 7 Tage verbleibend |
| Rot   | ≤ 3 Tage oder abgelaufen |

---

## Web-Interface

Aufrufbar unter `http://<IP-Adresse>` im lokalen Netzwerk.

- **Suche**: Produkt nach Name, Marke oder Barcode filtern
- **Filter**: Alle / Bald ablaufend / Abgelaufen / OK
- **Sortierung**: Klick auf Spaltenüberschrift
- **Löschen**: Artikel einzeln entfernen
- **CSV Export**: Komplettes Inventar als CSV herunterladen

---

## API-Endpunkte

| Methode | Pfad | Beschreibung |
|---------|------|--------------|
| GET | `/api/inventory` | Alle Artikel als JSON |
| GET | `/api/stats` | Statistiken |
| DELETE | `/api/item?barcode=X&expiry=YYYY-MM-DD` | Artikel löschen |

---

## Datenbank

Verwendet [Open Food Facts](https://world.openfoodfacts.org/) — eine freie, offene Lebensmitteldatenbank mit über 3 Millionen Produkten. Besonders gut abgedeckt: Deutschland, Österreich, Schweiz.

Offline-Betrieb ist möglich: Produkte werden dann als "Unbekannt (Barcode)" gespeichert, das Haltbarkeitsdatum kann trotzdem eingetragen werden.

---

## Datenspeicherung

Das Inventar wird als JSON-Datei im LittleFS-Dateisystem des ESP32 gespeichert:

```json
{
  "items": [
    {
      "barcode": "4001234567890",
      "name": "Alpenmilch 3,5% Fett",
      "brand": "Alpenmilch",
      "expiry": "2025-02-15",
      "added": "2025-01-10",
      "qty": 2
    }
  ]
}
```

Gleicher Barcode + gleiches MHD → Menge wird erhöht (kein Duplikat).

---

## Pinbelegung anpassen

Alle Pins sind zentral in `src/config.h` definiert. Display-Pins werden über `build_flags` in `platformio.ini` konfiguriert.
