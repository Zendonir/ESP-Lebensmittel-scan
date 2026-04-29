#pragma once
#include "FoodAPI.h"
#include <vector>

class FoodCache {
public:
    bool begin();
    ProductInfo get(const String &barcode) const;
    void put(const ProductInfo &p);
    int  count() const { return _entries.size(); }

private:
    struct CacheEntry {
        String barcode, name, brand, quantity, category;
    };
    std::vector<CacheEntry> _entries;
    bool load();
    bool save();
};

extern FoodCache foodCache;
