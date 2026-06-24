#include "firmware_manager.h"

#include "thread_controller.h"
#include "zigbee_proxy_controller.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "FirmwareManager";

// Static controller instances — exactly one is active at any time.
// Zigbee USB and NET share one ZigbeeProxyController; mode is passed at construction.
static ThreadController      threadCtrl;
static ZigbeeProxyController zigbeeUsbCtrl{DeviceMode::ZIGBEE_USB};
static ZigbeeProxyController zigbeeNetCtrl{DeviceMode::ZIGBEE_NET};

FirmwareManager::FirmwareManager()
    : activeMode{DeviceMode::THREAD}
{}

esp_err_t FirmwareManager::start(DeviceMode mode)
{
    ESP_LOGI(TAG, "Starting mode %d", static_cast<int>(mode));

    switch (mode) {
        case DeviceMode::THREAD:
            ESP_RETURN_ON_ERROR(threadCtrl.start(), TAG, "ThreadController start failed");
            break;

        case DeviceMode::ZIGBEE_USB:
            ESP_RETURN_ON_ERROR(zigbeeUsbCtrl.start(), TAG, "ZigbeeProxyController (USB) start failed");
            break;

        case DeviceMode::ZIGBEE_NET:
            ESP_RETURN_ON_ERROR(zigbeeNetCtrl.start(), TAG, "ZigbeeProxyController (NET) start failed");
            break;

        case DeviceMode::ZIGBEE_ROUTER:
            // RCP runs standalone — ESP has nothing to start
            ESP_LOGI(TAG, "ZIGBEE_ROUTER mode: RCP is self-contained, no stack to start");
            break;
    }

    activeMode = mode;
    return ESP_OK;
}

DeviceMode FirmwareManager::getActiveMode()
{
    return activeMode;
}
