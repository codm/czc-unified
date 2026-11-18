#ifndef ICONFIGSTORE_H_
#define ICONFIGSTORE_H_

#include "esp_err.h"
#include "WifiCreds.h"

/**
 * @brief Persistent network configuration (WiFi creds + "verified" flag).
 *
 * Implementations must be thread-safe (httpd task writes, NSM task reads).
 */
class IConfigStore
{
public:
    virtual ~IConfigStore() = default;

    /**
     * @brief Load stored credentials.
     * @param[out] out Filled only on success.
     * @return false if nothing stored or stored data is invalid.
     */
    virtual bool loadCreds(WifiCreds& out) const = 0;

    /**
     * @brief Store new credentials and reset verified to false.
     * @return ESP_OK, ESP_ERR_INVALID_ARG for invalid creds, or a storage error.
     */
    virtual esp_err_t saveCreds(const WifiCreds& creds) = 0;

    /** @brief true if the stored creds connected successfully at least once. */
    virtual bool isVerified() const = 0;

    /** @brief Set/clear the verified flag. */
    virtual esp_err_t setVerified(bool verified) = 0;
};

#endif // ICONFIGSTORE_H_
