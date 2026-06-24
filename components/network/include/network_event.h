#ifndef NETWORK_EVENT_H_
#define NETWORK_EVENT_H_

#include "esp_event.h"

// base NetworkStateMachine on events
ESP_EVENT_DECLARE_BASE(NETWORK_EVENT);
enum {
    NETWORK_EVENT_CONFIG_UPDATED,       ///< WiFi credentials or IP config changed — NSM reinitialises WiFi
    NETWORK_EVENT_ETH_CONFIG_UPDATED,   ///< Ethernet IP config changed — NSM reinitialises Ethernet
    NETWORK_EVENT_INIT_TIMEOUT,
    NETWORK_EVENT_RETRY_TIMEOUT,
    NETWORK_EVENT_WIFI_DISCONNECTED
};

#endif /* NETWORK_EVENT_H_ */