#include "app_nvs.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char* TAG = "AppNvs";

constexpr const char* NVS_NAMESPACE    = "sys_cfg";
constexpr const char* KEY_DEVICE_SETUP  = "device_setup";
constexpr const char* KEY_DEVICE_MODE   = "device_mode";

namespace AppNvs {

bool readDeviceSetup()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t val{0};
    nvs_get_u8(handle, KEY_DEVICE_SETUP, &val);
    nvs_close(handle);
    return static_cast<bool>(val);
}

esp_err_t writeDeviceSetup(bool value)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle),
                        TAG, "NVS open failed");
    esp_err_t ret{nvs_set_u8(handle, KEY_DEVICE_SETUP, static_cast<uint8_t>(value))};
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

DeviceMode readDeviceMode()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return DeviceMode::THREAD;
    }
    int32_t val{static_cast<int32_t>(DeviceMode::THREAD)};
    nvs_get_i32(handle, KEY_DEVICE_MODE, &val);
    nvs_close(handle);
    return static_cast<DeviceMode>(val);
}

esp_err_t writeDeviceMode(DeviceMode mode)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle),
                        TAG, "NVS open failed");
    esp_err_t ret{nvs_set_i32(handle, KEY_DEVICE_MODE, static_cast<int32_t>(mode))};
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

} // namespace AppNvs
