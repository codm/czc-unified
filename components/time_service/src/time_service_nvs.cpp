#include "time_service_nvs.h"

#include "nvs_flash.h"
#include "esp_log.h"

namespace
{
    constexpr const char* TAG = "time_service_nvs";
    constexpr const char* DEFAULTS_NAMESPACE = "time_service_defs";

    constexpr const char* TIMEZONE_KEY = "tz";
    constexpr const char* TIMESERVER_KEY = "ts";

    esp_err_t readString(const char* key, char* value, size_t& length)
    {
        if (key == nullptr || value == nullptr)
        {
            return ESP_ERR_INVALID_ARG;
        }

        nvs_handle_t handle;
        esp_err_t ret{nvs_open(DEFAULTS_NAMESPACE, NVS_READONLY, &handle)};
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", DEFAULTS_NAMESPACE, esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_get_str(handle, key, value, &length);
        nvs_close(handle);

        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to read key '%s': %s", key, esp_err_to_name(ret));
        }
        return ret;
    }

    esp_err_t writeString(const char* key, const char* value)
    {
        if (key == nullptr || value == nullptr)
        {
            return ESP_ERR_INVALID_ARG;
        }

        nvs_handle_t handle;
        esp_err_t ret{nvs_open(DEFAULTS_NAMESPACE, NVS_READWRITE, &handle)};
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", DEFAULTS_NAMESPACE, esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_set_str(handle, key, value);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to write key '%s': %s", key, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }

        ret = nvs_commit(handle);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to commit key '%s': %s", key, esp_err_to_name(ret));
        }

        nvs_close(handle);
        return ret;
    }
} // namespace

namespace TimeServiceNvs
{
    esp_err_t readDefaultTimezone(char* tz, size_t& length)
    {
        return readString(TIMEZONE_KEY, tz, length);
    }

    esp_err_t writeDefaultTimezone(const char* tz)
    {
        return writeString(TIMEZONE_KEY, tz);
    }

    esp_err_t readDefaultTimeServer(char* ts, size_t& length)
    {
        return readString(TIMESERVER_KEY, ts, length);
    }

    esp_err_t writeDefaultTimeServer(const char* ts)
    {
        return writeString(TIMESERVER_KEY, ts);
    }

} // namespace TimeServiceNvs
