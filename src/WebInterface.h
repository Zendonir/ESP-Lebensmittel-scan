#pragma once
#include <ESPAsyncWebServer.h>
#include "Inventory.h"

class WebInterface {
public:
    WebInterface(Inventory &inventory);
    void begin();

private:
    AsyncWebServer _server;
    Inventory &_inventory;

    void handleInventoryAPI(AsyncWebServerRequest *request);
    void handleDeleteAPI(AsyncWebServerRequest *request);
    void handleStatsAPI(AsyncWebServerRequest *request);
};
