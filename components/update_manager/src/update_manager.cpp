#include "update_manager.h"

#include "rcp_updater.h"
#include "ota_updater.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "UpdateManager";

static RcpUpdater rcpUpdater;
static OtaUpdater otaUpdater;

UpdateManager::UpdateManager()
    : busy{false}
{}

esp_err_t UpdateManager::flashRcp(const char* url)
{
    if (busy) {
        ESP_LOGE(TAG, "Update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    busy = true;

    ESP_LOGI(TAG, "Starting RCP flash: %s", url);
    esp_err_t err{rcpUpdater.flash(url)};

    busy = false;
    return err;
}

esp_err_t UpdateManager::flashEsp(const char* url)
{
    if (busy) {
        ESP_LOGE(TAG, "Update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    busy = true;

    // OtaUpdater spawns a task and reboots on success — busy is not cleared
    // here intentionally: the reboot will reset all state.
    ESP_LOGI(TAG, "Starting ESP OTA: %s", url);
    esp_err_t err{otaUpdater.flash(url)};

    if (err != ESP_OK) {
        busy = false;
    }
    return err;
}
