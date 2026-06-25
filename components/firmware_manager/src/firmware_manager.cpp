#include "firmware_manager.h"

#include "thread_controller.h"
#include "zigbee_proxy_controller.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "FirmwareManager";

FirmwareManager::FirmwareManager()
    : protocol{nullptr}, activeMode{DeviceMode::THREAD}
{}

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
