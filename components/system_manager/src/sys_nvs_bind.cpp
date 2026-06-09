#include "sys_nvs_bind.h"

static const char* TAG = "sys-nvs-bind";

esp_err_t SysNvsBinding::readRcpFlashConfig(RcpFlashConfig& config)
{
    nvs_handle_t handle;
    size_t urlLen = sizeof(config.url);
    uint8_t pending = 0;

    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_SYS_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_get_str(handle, NVS_KEY_RCP_URL, config.url, &urlLen);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read rcp_url: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_get_u8(handle, NVS_KEY_RCP_PENDING, &pending);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read rcp_pending: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    config.pendingFlash = (bool)pending;

    ESP_LOGI(TAG, "RCP flash config read from NVS (url: %s, pending: %d)", config.url, config.pendingFlash);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t SysNvsBinding::writeRcpFlashConfig(const RcpFlashConfig& config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_SYS_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(handle, NVS_KEY_RCP_URL, config.url);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write rcp_url: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_set_u8(handle, NVS_KEY_RCP_PENDING, (uint8_t)config.pendingFlash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write rcp_pending: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "RCP flash config written to NVS (url: %s, pending: %d)", config.url, config.pendingFlash);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t SysNvsBinding::clearRcpFlashConfig()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_SYS_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_all(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS namespace: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS erase: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "RCP flash config cleared from NVS");

cleanup:
    nvs_close(handle);
    return ret;
}

bool SysNvsBinding::rcpFlashPending()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }

    uint8_t pending = 0;
    ret = nvs_get_u8(handle, NVS_KEY_RCP_PENDING, &pending);
    nvs_close(handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read pending from NVS");
        return false;
    }

    return (bool)pending;
}

bool SysNvsBinding::deviceSetup()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {

        ESP_LOGW(TAG, "Error while trying to open NvsDeviceSetup");
        nvs_close(handle);
        return false;
    }

    uint8_t deviceSetup = 0;
    ret = nvs_get_u8(handle, NVS_KEY_DEVICE_SETUP, &deviceSetup);
    nvs_close(handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read deviceSetup from NVS");
        return false;
    }

    return (bool)deviceSetup;
}

esp_err_t SysNvsBinding::writeNvsDeviceSetup(bool isDeviceSetup)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_SYS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error while trying to open NvsDeviceSetup - READWRITE | Error: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_set_u8(handle, NVS_KEY_DEVICE_SETUP, (uint8_t)isDeviceSetup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write deviceSetup: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    return ESP_OK;

    cleanup:
    nvs_close(handle);
    return ESP_FAIL;
}
