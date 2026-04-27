#pragma once
#include <Arduino.h>
#include <vector>

struct CustomProduct {
    String name;
    String brand;
    String barcode;
    String category;   // muss einem der CATEGORIES[i].name entsprechen
};

class CustomProducts {
public:
    bool begin();
    bool load();
    bool save();

    bool add(const CustomProduct &p);
    bool remove(int index);
    bool update(int index, const CustomProduct &p);

    const std::vector<CustomProduct>& items() const { return _items; }
    int count() const { return _items.size(); }

    // Gefilterte Liste nach Kategorie
    std::vector<CustomProduct> byCategory(const String &cat) const;

private:
    std::vector<CustomProduct> _items;
};
