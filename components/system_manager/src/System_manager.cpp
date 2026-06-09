#include "System_manager.h"
#include "sys_nvs_bind.h"

const char* System_manager::TAG = "System-Manager";

System_manager::System_manager(Rcp_interface _rcp_interface, uart_port_t _rcp_uart_num)
{
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler, NULL));
    this->rcp_interface = _rcp_interface;
    this->rcp_uart_num = _rcp_uart_num;
    this->ota_event_group = xEventGroupCreate();
}

System_manager::~System_manager()
{
}

static esp_err_t flash_esp_cb(void *ctx, const char *url)
{
    static_cast<System_manager *>(ctx)->flashEspFirmware(url);
    return ESP_OK;
}

static esp_err_t flash_rcp_cb(void *ctx, const char *url)
{
    static_cast<System_manager *>(ctx)->initRcpFirmwareFlash(url);
    return ESP_OK;
}

esp_err_t System_manager::initThread()
{
    system_flash_callbacks_t cbs = {
        .flash_esp = flash_esp_cb,
        .flash_rcp = flash_rcp_cb,
        .ctx       = this,
    };
    return thread_controller.init(&cbs);
}

esp_err_t System_manager::startThread()
{
    return thread_controller.start();
}

void System_manager::flashEspFirmware(const char* url)
{
    EspFlashConfig* config = new EspFlashConfig();
    strlcpy(config->url, url, sizeof(config->url));
    config->self = this;
    config->url[sizeof(config->url) - 1] = '\0';
    ESP_LOGD(TAG, "Starting ESP Update Task...");
    xTaskCreate(ota_update_task, "ota_update_task", 1024 * 8, config, 5, NULL);
    ESP_LOGD(TAG, "Update Task started!");
}

void System_manager::flashRcpFirmware(const char* url)
{
    if (thread_controller.is_running()) {
        ESP_LOGD(TAG, "Thread is running, stopping it now!");
        esp_err_t ret = thread_controller.stop();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop Thread: %s — aborting RCP flash", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "Thread stopped successfully");
    }
    ESP_LOGD(TAG, "Init RCP Update");
    rcp_interface.rcp_update_init(rcp_uart_num);
    ESP_LOGD(TAG, "Starting RCP Update Task...");
    esp_err_t err = rcp_interface.rcp_update_start(url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RCP update: %s", esp_err_to_name(err));
        return;
    }

    EventBits_t bits = xEventGroupWaitBits(
        rcp_interface.get_update_event_group(),
        RCP_UPDATE_SUCCESS_BIT | RCP_UPDATE_FAIL_BIT,
        pdTRUE,   // clear bits after read
        pdFALSE,  // one bit is enough
        pdMS_TO_TICKS(portMAX_DELAY)
    );

    SysNvsBinding::clearRcpFlashConfig();

    if (bits & RCP_UPDATE_SUCCESS_BIT) {
        ESP_LOGI(TAG, "RCP firmware update successful — restarting");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "RCP firmware update failed or timed out, REBOOT REQUIRED!");
    }
}

void System_manager::initRcpFirmwareFlash(const char* url)
{
    RcpFlashConfig config;
    strlcpy(config.url, url, sizeof(config.url));
    config.pendingFlash = true;

    esp_err_t ret = SysNvsBinding::writeRcpFlashConfig(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write RCP flash config to NVS: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "RCP flash scheduled, restarting...");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

bool System_manager::isRcpFlashPending()
{
    return SysNvsBinding::rcpFlashPending();
}

void System_manager::flashRcpFirmwareWhenConfigured()
{
    if (!SysNvsBinding::rcpFlashPending()) {
        return;
    }

    RcpFlashConfig config;
    esp_err_t ret = SysNvsBinding::readRcpFlashConfig(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read RCP flash config from NVS: %s", esp_err_to_name(ret));
        SysNvsBinding::clearRcpFlashConfig();
        return;
    }

    ESP_LOGI(TAG, "Pending RCP flash found, starting update from: %s", config.url);
    flashRcpFirmware(config.url);
}

void System_manager::ota_update_task(void* pvParameter)
{
    auto* config = static_cast<EspFlashConfig*>(pvParameter);
    auto* self   = config->self;
    ESP_LOGD(TAG, "OTA task started, URL: %s", config->url);

    esp_err_t err            = ESP_OK;
    esp_err_t ota_finish_err = ESP_OK;

    // config
    esp_http_client_config_t http_config = {
        .url                   = config->url,
        .timeout_ms            = EXAMPLE_OTA_RECV_TIMEOUT_MS,
        .max_redirection_count = 5,
        .buffer_size           = 1024 * 8,
        .buffer_size_tx        = 1024 * 8,
        .crt_bundle_attach     = esp_crt_bundle_attach,
        .keep_alive_enable     = false,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    // begin ota
    esp_https_ota_handle_t ota_handle = nullptr;
    err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
        delete config;
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
        ESP_LOGE(TAG, "Download incomplete");
        goto ota_abort;
    }
    ESP_LOGD(TAG, "Download complete!");

    // finish ota
    ota_finish_err = esp_https_ota_finish(ota_handle);
    ota_handle = nullptr;

    if (ota_finish_err != ESP_OK) {
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "OTA finish: Image validation failed!");
        } else {
            // ESP_LOGE(TAG, "esp_https_ota_finish Fehler: %s", esp_err_to_name(ota_finish_err));
        }
        xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
        vTaskDelete(NULL);
        return;
    }

    // restart esp
    ESP_LOGI(TAG, "OTA update completed - Rebooting...");
    // xEventGroupSetBits(self->ota_event_group, OTA_SUCCESS_BIT);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

ota_abort:
    if (ota_handle != nullptr) {
        esp_https_ota_abort(ota_handle);
    }
    xEventGroupSetBits(self->ota_event_group, OTA_FAIL_BIT);
    delete config;
    vTaskDelete(NULL);
}

bool System_manager::isDeviceSetup()
{
    return SysNvsBinding::deviceSetup();
}

esp_err_t System_manager::writeDeviceSetup(bool isDeviceSetup)
{
    return SysNvsBinding::writeNvsDeviceSetup(isDeviceSetup);
}

void System_manager::ota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static char* TAG = "OTA";
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_REVISION:
                ESP_LOGI(TAG, "Verifying chip revision of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}
