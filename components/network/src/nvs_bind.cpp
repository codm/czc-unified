#include "nvs_bind.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "nvs-bind";

#define NVS_KEY_SSID       "ssid"
#define NVS_KEY_PASSWORD   "password"
#define NVS_KEY_CONFIGURED "configured"

esp_err_t NvsBinding::readNetworkConfig(NetworkConfig& config)
{
    nvs_handle_t handle;
    size_t ssidLen     = sizeof(config.ssid);
    size_t passwordLen = sizeof(config.password);
    uint8_t configured = 0;

    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_WIFI_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_get_str(handle, NVS_KEY_SSID, config.ssid, &ssidLen);
    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read ssid: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_get_str(handle, NVS_KEY_PASSWORD, config.password, &passwordLen);
    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read password: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_get_u8(handle, NVS_KEY_CONFIGURED, &configured);
    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read configured flag: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    config.wifiConfigured = (bool)configured;

    ESP_LOGI(TAG, "Network config read from NVS (ssid: %s)", config.ssid);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t NvsBinding::writeNetworkConfig(const NetworkConfig& config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_WIFI_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(handle, NVS_KEY_SSID, config.ssid);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write ssid: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_set_str(handle, NVS_KEY_PASSWORD, config.password);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_set_u8(handle, NVS_KEY_CONFIGURED, (uint8_t)config.wifiConfigured);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write configured flag: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Network config written to NVS (ssid: %s)", config.ssid);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t NvsBinding::clearNetworkConfig()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_WIFI_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_all(handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase NVS namespace: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS erase: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Network config cleared from NVS");

cleanup:
    nvs_close(handle);
    return ret;
}

bool NvsBinding::networkConfigExists()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if(ret != ESP_OK)
    {
        return false;
    }

    uint8_t configured = 0;
    ret = nvs_get_u8(handle, NVS_KEY_CONFIGURED, &configured);
    nvs_close(handle);

    if(ret != ESP_OK)
    {
        return false;
    }

    return configured == 1;
}
