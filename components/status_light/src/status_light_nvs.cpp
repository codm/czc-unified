#include "status_light_nvs.h"

#include "nvs_flash.h"
#include "esp_log.h"

namespace
{
    constexpr const char* TAG {"status_light_nvs"};
    constexpr const char* NVS_NAMESPACE {"status_lights_defs"};
    constexpr const char* OVERWRITES_KEY {"ov"};
    constexpr const char* NIGHT_MODE_KEY {"nm"};

    esp_err_t readBlob(const char* key, void* value, size_t& length) 
    {
        if (key == nullptr || value == nullptr)
        {
            return ESP_ERR_INVALID_ARG;
        }

        nvs_handle_t handle;
        esp_err_t ret{nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle)};
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_get_blob(handle, key, value, &length);
        nvs_close(handle);

        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to read key '%s': %s", key, esp_err_to_name(ret));
        }
        return ret;
    }

    esp_err_t writeBlob(const char* key, const void* value, size_t& length)
    {
        if (key == nullptr || value == nullptr)
        {
            return ESP_ERR_INVALID_ARG;
        }

        nvs_handle_t handle;
        esp_err_t ret{nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle)};
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_set_blob(handle, key, value, length);
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
}
namespace StatusLightNvs
{
    esp_err_t readOverwrites(void* ovwrts, size_t &length)
    {
        return readBlob(OVERWRITES_KEY, ovwrts, length);
    }
    esp_err_t writeOverwrites(const void *ovwrts, size_t& length)
    {
        return writeBlob(OVERWRITES_KEY, ovwrts, length);
    }
    esp_err_t readNightModeState(void *nightModeState, size_t &length)
    {
        return readBlob(NIGHT_MODE_KEY, nightModeState, length);
    }
    esp_err_t writeNightModeState(void *nightModeState, size_t& length)
    {
        return writeBlob(NIGHT_MODE_KEY, nightModeState, length);
    }
}