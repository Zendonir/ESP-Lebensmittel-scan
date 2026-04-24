#pragma once
#include <Arduino.h>
#include <vector>

struct CustomProduct {
    String name;
    String brand;
    String barcode;   // leer wenn kein Barcode vorhanden
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

private:
    std::vector<CustomProduct> _items;
};
