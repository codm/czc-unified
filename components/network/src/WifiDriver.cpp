
#include "WifiDriver.h"

#include "esp_wifi.h"
#include "esp_log.h"

WifiDriver::~WifiDriver()
{
    std::lock_guard lock(mtx_);
    if (inited_) {
        esp_wifi_stop();
        esp_wifi_deinit();
        inited_ = false;
    }
}

esp_err_t WifiDriver::acquire(Role role)
{
    std::lock_guard lock(mtx_);
    const uint8_t bit = static_cast<uint8_t>(role);
    if (roles_ & bit) return ESP_OK; // already registered

    if (!inited_) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_wifi_init(&cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
            return err;
        }

        esp_wifi_set_storage(WIFI_STORAGE_RAM); // creds are persisted by IConfigStore
        inited_ = true;
        ESP_LOGD(TAG, "driver initialised");
    }

    const uint8_t next = roles_ | bit;
    esp_err_t err = applyMode(next);
    if (err != ESP_OK) {
        if (roles_ == 0) { esp_wifi_deinit(); inited_ = false; }
        return err;
    }
    roles_ = next;
    return ESP_OK;
}

void WifiDriver::release(Role role)
{
    std::lock_guard lock(mtx_);
    const uint8_t bit = static_cast<uint8_t>(role);
    if (!(roles_ & bit)) return;

    roles_ &= static_cast<uint8_t>(~bit);
    if (roles_ == 0) {
        esp_wifi_stop();
        esp_wifi_deinit();
        inited_ = false;
        ESP_LOGD(TAG, "last role released, driver deinitialised");
    } else {
        applyMode(roles_);
    }
}

bool WifiDriver::has(Role role) const
{
    std::lock_guard lock(mtx_);
    return roles_ & static_cast<uint8_t>(role);
}

esp_err_t WifiDriver::applyMode(uint8_t roles)
{
    const wifi_mode_t mode = toMode(roles);
    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_mode %s failed: %s", modeName(mode), esp_err_to_name(err));
        return err;
    }
    ESP_LOGD(TAG, "mode %s", modeName(mode));
    if (roles_ == 0) {          // first role: driver not running yet
        err = esp_wifi_start();
        if (err != ESP_OK) ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
    }
    return err;
}

wifi_mode_t WifiDriver::toMode(uint8_t roles)
{
    const bool sta = roles & static_cast<uint8_t>(Role::Sta);
    const bool ap  = roles & static_cast<uint8_t>(Role::Ap);
    if (sta && ap) return WIFI_MODE_APSTA;
    if (ap)        return WIFI_MODE_AP;
    if (sta)       return WIFI_MODE_STA;
    return WIFI_MODE_NULL;
}

const char* WifiDriver::modeName(wifi_mode_t mode)
{
    switch (mode) {
    case WIFI_MODE_STA:   return "STA";
    case WIFI_MODE_AP:    return "AP";
    case WIFI_MODE_APSTA: return "APSTA";
    default:              return "NULL";
    }
}
