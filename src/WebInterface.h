#pragma once
#include <ESPAsyncWebServer.h>
#include "Inventory.h"
#include "CustomProducts.h"
#include "StorageStats.h"
#include "ShoppingList.h"

class WebInterface {
public:
    WebInterface(Inventory &inventory, CustomProducts &customProducts,
                 StorageStats &stats, ShoppingList &shopping);
    void begin();
    void loop();

private:
    AsyncWebServer _server;
    Inventory      &_inventory;
    CustomProducts &_customProducts;
    StorageStats   &_stats;
    ShoppingList   &_shopping;
};
