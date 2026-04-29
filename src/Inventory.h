#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct InventoryItem {
    String barcode;
    String name;
    String brand;
    String expiryDate;    // "YYYY-MM-DD"
    String addedDate;     // "YYYY-MM-DD"
    int    quantity;
    String labelBarcode;  // generierter Code für Etikett, z.B. "L00001"
};

class Inventory {
public:
    bool begin();
    bool load();
    bool save();

    bool addItem(const InventoryItem &item);
    bool removeItem(const String &barcode, const String &expiryDate);
    bool removeByLabel(const String &labelBarcode);
    bool incrementItem(const String &barcode, const String &expiryDate);

    // Suche anhand des Etikett-Barcodes (für Auslagerung)
    const InventoryItem *findByLabel(const String &labelBarcode) const;

    const std::vector<InventoryItem>& items() const { return _items; }
    int count() const { return _items.size(); }

    // Gibt Anzahl der Artikel zurück die in <= days Tagen ablaufen
    int expiringIn(int days) const;
    int expiredCount() const;

private:
    std::vector<InventoryItem> _items;

    int daysUntilExpiry(const String &dateStr) const;
};
