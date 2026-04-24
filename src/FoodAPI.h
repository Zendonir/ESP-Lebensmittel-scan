#pragma once
#include <Arduino.h>

struct ProductInfo {
    bool found;
    String barcode;
    String name;
    String brand;
    String quantity;   // z.B. "1 l" oder "500 g"
    String category;
};

class FoodAPI {
public:
    ProductInfo lookup(const String &barcode);

private:
    String httpGet(const String &url);
};
