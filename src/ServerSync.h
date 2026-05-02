#pragma once
#include <Arduino.h>
#include <vector>
#include "Inventory.h"

class ServerSync {
public:
    struct Config {
        String serverUrl;    // z.B. "http://192.168.1.100:8000"
        String deviceId;     // MAC-Adresse
        String deviceName;
        String deviceRoom;
        int    householdId = 0;
    };

    bool begin(const Config &cfg);
    void loop();

    void onItemAdded(const InventoryItem &item);
    void onItemRemoved(const InventoryItem &item, int storageDays);

    static Config  loadConfig();
    static void    saveConfig(const Config &cfg);
    bool isConfigured() const { return !_cfg.serverUrl.isEmpty(); }

private:
    Config _cfg;
    bool   _registered   = false;
    unsigned long _lastRetry = 0;

    struct PendingEvent {
        String type;
        String payload;
    };
    std::vector<PendingEvent> _queue;

    bool registerDevice();
    bool httpPost(const String &path, const String &body);
    void processQueue();
    String buildItemPayload(const String &type, const InventoryItem &item, int storageDays = -1);
};

extern ServerSync serverSync;
