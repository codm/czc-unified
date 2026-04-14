#include "System_manager.h"

const char* System_manager::TAG = "System-Manager";

System_manager::System_manager()
{
}

System_manager::~System_manager()
{
}

void System_manager::flashEspFirmware()
{
    ESP_LOGD(TAG, "Starting ESP Update Task...");
    xTaskCreate(ota_update_task, "ota_update_task", 1024 * 8, this, 5, NULL);
    ESP_LOGD(TAG, "Update Task started!");
}

void System_manager::ota_update_task(void* pvParameter)
{
    auto* self = static_cast<System_manager*>(pvParameter);
    ESP_LOGD(TAG, "OTA task started, URL: %s", self->esp_download_url);

    esp_err_t err            = ESP_OK;
    esp_err_t ota_finish_err = ESP_OK;

    // config
    esp_http_client_config_t http_config = {
        .url               = self->esp_download_url,
        .timeout_ms        = EXAMPLE_OTA_RECV_TIMEOUT_MS,
        .buffer_size       = 1024 * 8,
        .buffer_size_tx    = 1024 * 8,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true, 
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .partial_http_download = true,
        .max_http_request_size = 1024 * 4,
    };

    // begin ota
    esp_https_ota_handle_t ota_handle = nullptr;
    err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGD(TAG, "OTA begin OK");

    // download
    ESP_LOGD(TAG, "Starting download...");
    while (true) {
        err = esp_https_ota_perform(ota_handle);

        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            ESP_LOGD(TAG, "read bytes: %zu", esp_https_ota_get_image_len_read(ota_handle));
            continue;
        }
        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform Fehler: %s", esp_err_to_name(err));
        goto ota_abort;
    }

    // test download
    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ESP_LOGE(TAG, "Download unvollständig");
        goto ota_abort;
    }
    ESP_LOGD(TAG, "Download vollständig");

    // finish ota
    ota_finish_err = esp_https_ota_finish(ota_handle);
    ota_handle = nullptr;

    if (ota_finish_err != ESP_OK) {
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "OTA finish: Image Validierung fehlgeschlagen");
        } else {
            // ESP_LOGE(TAG, "esp_https_ota_finish Fehler: %s", esp_err_to_name(ota_finish_err));
        }
        xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
        vTaskDelete(NULL);
        return;
    }

    // restart esp
    ESP_LOGI(TAG, "OTA erfolgreich - Neustart...");
    // xEventGroupSetBits(self->ota_event_group, OTA_SUCCESS_BIT);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

ota_abort:
    if (ota_handle != nullptr) {
        esp_https_ota_abort(ota_handle);
    }
    xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
    vTaskDelete(NULL);
}
