#pragma once
#include "Inventory.h"
#include <vector>

struct StorageStat {
    String name;
    String category;
    String addedDate;
    String removedDate;
    int    storageDays;
};

class StorageStats {
public:
    bool begin();
    bool record(const InventoryItem &item, const String &removedDate, int storageDays);
    const std::vector<StorageStat>& items() const { return _items; }
    int  count() const { return _items.size(); }

private:
    std::vector<StorageStat> _items;
    bool load();
    bool save();
};

extern StorageStats storageStats;
