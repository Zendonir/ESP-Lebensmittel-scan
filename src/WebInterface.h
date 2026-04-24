#pragma once
#include <ESPAsyncWebServer.h>
#include "Inventory.h"
#include "CustomProducts.h"

class WebInterface {
public:
    WebInterface(Inventory &inventory, CustomProducts &customProducts);
    void begin();

private:
    AsyncWebServer _server;
    Inventory      &_inventory;
    CustomProducts &_customProducts;
};
