#ifndef INETWORKCONFIG_H_
#define INETWORKCONFIG_H_

#include "esp_err.h"
#include "WifiCreds.h"

/** @brief Write access to the network configuration (for the web server). */
class INetworkConfig {
public:
    virtual ~INetworkConfig() = default;

    /**
     * @brief Persist new STA credentials and trigger a reconnect.
     *
     * Callable from any task (typically httpd). Stores the creds (verified
     * is reset) and notifies the NSM asynchronously. Over ETH the creds are
     * only stored and used once the cable is unplugged.
     *
     * @return ESP_OK, ESP_ERR_INVALID_ARG for invalid creds, a storage error,
     *         or ESP_ERR_NO_MEM if the NSM queue is full (creds are stored anyway).
     */
    virtual esp_err_t setWifiCredentials(const WifiCreds& creds) = 0;
};

#endif // INETWORKCONFIG_H_
