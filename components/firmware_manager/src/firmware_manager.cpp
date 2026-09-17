#include "firmware_manager.h"

#include "thread_controller.h"
#include "zigbee_proxy_controller.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "FirmwareManager";

FirmwareManager::FirmwareManager()
    : protocol{nullptr}, activeMode{DeviceMode::THREAD}
{}

esp_err_t FirmwareManager::init()
{
    rcpLedQueue = xQueueCreate(1, sizeof(bool));
    ESP_RETURN_ON_FALSE(rcpLedQueue, ESP_ERR_NO_MEM, TAG, "rcpLedQueue create failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(RCP_LED_EVENT, ESP_EVENT_ANY_ID, &ledEventHandler, this),
        TAG, "RCP_LED_EVENT handler register failed");

    BaseType_t ok = xTaskCreate(&rcpLedTaskFunc, "rcp_led", 3072, this, 3, &rcpLedTask);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "rcpLedTask create failed");

    return ESP_OK;
}

void FirmwareManager::ledEventHandler(void* arg, esp_event_base_t /*event_base*/,
                                      int32_t event_id, void* /*event_data*/)
{
    auto* self = static_cast<FirmwareManager*>(arg);
    bool  ledState = (event_id != 0);
    xQueueOverwrite(self->rcpLedQueue, &ledState);
}

void FirmwareManager::rcpLedTaskFunc(void* arg)
{
    auto* self = static_cast<FirmwareManager*>(arg);
    bool  ledState;

    while (true) {
        if (xQueueReceive(self->rcpLedQueue, &ledState, portMAX_DELAY) == pdTRUE && self->protocol) {
            esp_err_t err = self->protocol->setRcpLed(ledState);
            if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(TAG, "setRcpLed failed: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t FirmwareManager::start(DeviceMode mode)
{
    ESP_LOGI(TAG, "Starting mode %d", static_cast<int>(mode));

    if (mode == DeviceMode::THREAD) 
    {
        protocol = std::make_unique<ThreadController>();
    }
    else 
    {
        protocol = std::make_unique<ZigbeeProxyController>(mode);
    }
    ESP_LOGD(TAG, "Protocol Interface setup! Starting...");

    ESP_RETURN_ON_ERROR(protocol->start(), TAG, "Error starting protocol");

    activeMode = mode;
    return ESP_OK;
}

esp_err_t FirmwareManager::stop()
{
    return protocol->stop();
}

DeviceMode FirmwareManager::getActiveMode()
{
    return activeMode;
}

esp_err_t FirmwareManager::resetRcp()
{
    ESP_RETURN_ON_FALSE(protocol, ESP_ERR_INVALID_STATE, TAG, "No protocol running");
    return protocol->resetRcp();
}

esp_err_t FirmwareManager::factoryReset()
{
    ESP_RETURN_ON_FALSE(protocol, ESP_ERR_INVALID_STATE, TAG, "No protocol running");
    return protocol->factoryReset();
}
