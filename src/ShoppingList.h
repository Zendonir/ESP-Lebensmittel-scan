#pragma once
#include <Arduino.h>
#include <vector>

struct ShoppingItem {
    String name;
    String category;
    String addedDate;
    bool   bought;
};

class ShoppingList {
public:
    bool begin();
    bool add(const String &name, const String &category);
    bool markBought(int index);
    bool remove(int index);
    bool clearBought();
    const std::vector<ShoppingItem>& items() const { return _items; }
    int  count() const { return _items.size(); }

private:
    std::vector<ShoppingItem> _items;
    bool load();
    bool save();
};

extern ShoppingList shoppingList;
