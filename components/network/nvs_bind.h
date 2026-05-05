#pragma once
#include "esp_err.h"

#define NVS_WIFI_NAMESPACE "wifi_cfg"

struct NetworkConfig {
    char ssid[32];
    char password[64];
    bool wifiConfigured;
};

namespace NvsBinding {
    esp_err_t readNetworkConfig(NetworkConfig& config);
    esp_err_t writeNetworkConfig(const NetworkConfig& config);
    esp_err_t clearNetworkConfig();
    bool networkConfigExists();
}
