#pragma once
#include <ESPAsyncWebServer.h>
#include "Inventory.h"
#include "CustomProducts.h"
#include "StorageStats.h"

class WebInterface {
public:
    WebInterface(Inventory &inventory, CustomProducts &customProducts, StorageStats &stats);
    void begin();
    void loop();   // muss in loop() aufgerufen werden (ElegantOTA)

private:
    AsyncWebServer _server;
    Inventory      &_inventory;
    CustomProducts &_customProducts;
    StorageStats   &_stats;
};
