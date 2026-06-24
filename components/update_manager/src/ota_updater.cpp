#include "ota_updater.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "OtaUpdater";

struct OtaTaskParams {
    char url[256];
};

OtaUpdater::OtaUpdater() {}

esp_err_t OtaUpdater::flash(const char* url)
{
    auto* params{new OtaTaskParams{}};
    strlcpy(params->url, url, sizeof(params->url));

    BaseType_t ret{xTaskCreate(otaTask, "ota_task", 1024 * 8, params, 5, nullptr)};
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        delete params;
        return ESP_FAIL;
    }
    return ESP_OK;
}

void OtaUpdater::otaTask(void* pvParameters)
{
    auto* params{static_cast<OtaTaskParams*>(pvParameters)};
    char url[256]{};
    strlcpy(url, params->url, sizeof(url));
    delete params;

    ESP_LOGI(TAG, "Starting OTA from: %s", url);

    esp_http_client_config_t httpCfg{};
    httpCfg.url                   = url;
    httpCfg.max_redirection_count = 5;
    httpCfg.buffer_size           = 1024 * 8;
    httpCfg.buffer_size_tx        = 1024 * 8;
    httpCfg.crt_bundle_attach     = esp_crt_bundle_attach;
    httpCfg.keep_alive_enable     = false;

    esp_https_ota_config_t otaCfg{};
    otaCfg.http_config = &httpCfg;

    esp_https_ota_handle_t otaHandle{nullptr};
    esp_err_t err{esp_https_ota_begin(&otaCfg, &otaHandle)};
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        err = esp_https_ota_perform(otaHandle);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) continue;
        break;
    }

    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(otaHandle)) {
        ESP_LOGE(TAG, "OTA download failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(otaHandle);
        vTaskDelete(nullptr);
        return;
    }

    err = esp_https_ota_finish(otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "OTA complete — rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}
