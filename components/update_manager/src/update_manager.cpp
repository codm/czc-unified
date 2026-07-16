#include "update_manager.h"

#include "rcp_updater.h"
#include "ota_updater.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "UpdateManager";

static RcpUpdater rcpUpdater;
static OtaUpdater otaUpdater;

namespace {
struct RcpFlashTaskParams {
    UpdateManager* self;
    char url[256];
};
} // namespace

UpdateManager::UpdateManager()
    : busy{false}, rcpFlashDone{xSemaphoreCreateBinary()}, rcpFlashResult{ESP_FAIL}
{}

esp_err_t UpdateManager::flashRcp(const char* url)
{
    if (busy) {
        ESP_LOGE(TAG, "Update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    busy = true;
    xSemaphoreTake(rcpFlashDone, 0);  // drain any stale signal from a previous run

    RcpFlashTaskParams* params{new RcpFlashTaskParams{this, {}}};
    strlcpy(params->url, url, sizeof(params->url));

    BaseType_t ret{xTaskCreate(rcpFlashTask, "rcp_flash_task", 1024 * 8, params, 5, nullptr)};
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RCP flash task");
        delete params;
        busy = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t UpdateManager::waitForRcpFlash(TickType_t timeout)
{
    if (xSemaphoreTake(rcpFlashDone, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return rcpFlashResult;
}

void UpdateManager::rcpFlashTask(void* pvParameters)
{
    auto* params{static_cast<RcpFlashTaskParams*>(pvParameters)};
    UpdateManager* self{params->self};
    char url[256]{};
    strlcpy(url, params->url, sizeof(url));
    delete params;

    ESP_LOGI(TAG, "Starting RCP flash: %s", url);
    self->rcpFlashResult = rcpUpdater.flash(url);
    self->busy = false;
    xSemaphoreGive(self->rcpFlashDone);
    vTaskDelete(nullptr);
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
