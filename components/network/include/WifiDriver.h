#ifndef WIFIDRIVER_H_
#define WIFIDRIVER_H_

#include <cstdint>
#include <mutex>
#include "esp_err.h"
#include "esp_wifi_types.h"

/**
 * @brief Shared esp_wifi driver for STA and AP.
 *
 * Links register their role; the driver derives STA/AP/APSTA from it.
 * Internal only, thread-safe.
 */
class WifiDriver {
public:
    enum class Role : uint8_t { Sta = 1 << 0, Ap = 1 << 1 }; // bit mask

    WifiDriver() = default;
    ~WifiDriver();

    WifiDriver(const WifiDriver&)            = delete;
    WifiDriver& operator=(const WifiDriver&) = delete;

    /**
     * @brief Register a role. First role: esp_wifi_init + start; otherwise mode switch.
     * @return ESP_OK (also if already registered) or an esp_wifi error.
     */
    esp_err_t acquire(Role role);

    /** @brief Unregister a role. Last role: esp_wifi_stop + deinit. */
    void      release(Role role);

    /** @brief true if @p role is registered. */
    bool has(Role role) const;

private:
    esp_err_t applyMode(uint8_t roles);
    static wifi_mode_t toMode(uint8_t roles);
    static const char* modeName(wifi_mode_t mode);

    static constexpr const char* TAG = "WifiDriver";

    mutable std::mutex mtx_;
    uint8_t roles_  = 0;
    bool inited_ = false;
};

#endif // WIFIDRIVER_H_
