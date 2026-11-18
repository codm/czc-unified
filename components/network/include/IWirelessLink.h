#ifndef IWIRELESSLINK_H_
#define IWIRELESSLINK_H_

#include "WifiCreds.h"
#include "ILink_hal.h"
#include "esp_err.h"

/** @brief Link that actively connects (WiFi STA). ETH is passive and lacks this. */
class IWirelessLink : public ILink_hal
{
public:
    /** @brief Set creds for the next connect(). Does not reconnect. */
    virtual void setCredentials(const WifiCreds& creds) = 0;

    /**
     * @brief Start a connection attempt (asynchronous).
     *
     * Result arrives as GotIp or ConnectFailed. A running attempt is
     * aborted first.
     * @return ESP_OK if started, ESP_ERR_INVALID_STATE if stopped,
     *         ESP_ERR_INVALID_ARG without creds, or an esp_wifi error.
     */
    virtual esp_err_t connect() = 0;

    /** @brief Abort attempt / disconnect. Intentional: posts no event. */
    virtual void disconnect() = 0;
};

#endif // IWIRELESSLINK_H_
