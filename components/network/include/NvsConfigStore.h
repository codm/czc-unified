#ifndef NVSCONFIGSTORE_H_
#define NVSCONFIGSTORE_H_

#include <mutex>
#include "nvs.h"
#include "IConfigStore.h"

/**
 * @brief IConfigStore on NVS (namespace "net").
 *
 * Thread-safe: saveCreds() is called from the httpd task, the rest from the NSM task.
 */
class NvsConfigStore final : public IConfigStore {
public:
    NvsConfigStore() = default;
    ~NvsConfigStore() override;

    NvsConfigStore(const NvsConfigStore&)            = delete;
    NvsConfigStore& operator=(const NvsConfigStore&) = delete;

    /**
     * @brief Open the NVS namespace. Call after nvs_flash_init().
     * @return ESP_OK (also if already open) or an nvs_open error.
     */
    esp_err_t init();

    bool      loadCreds(WifiCreds& out) const override;
    esp_err_t saveCreds(const WifiCreds& creds) override;
    bool      isVerified() const override;
    esp_err_t setVerified(bool verified) override;

    /**
     * @brief Erase all network settings (factory reset).
     * @return ESP_OK, ESP_ERR_INVALID_STATE if not open, or an NVS error.
     */
    esp_err_t clear();

private:
    static constexpr const char* TAG       = "net_cfg";
    static constexpr const char* kNs       = "net";
    static constexpr const char* kSsid     = "ssid";
    static constexpr const char* kPass     = "pass";
    static constexpr const char* kVerified = "verified";

    mutable std::mutex mtx_;
    nvs_handle_t       h_ = 0;
    bool               open_ = false;
};

#endif // NVSCONFIGSTORE_H_
