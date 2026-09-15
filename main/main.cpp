#include "NetworkStateMachine.h"
#include "status_light_manager.h"
#include "status_light_event.h"
#include "update_manager.h"
#include "firmware_manager.h"
#include "app_controller.h"
#include "esp_br_web.h"
#include "time_service.h"
#include "cron.h"

#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";

esp_err_t printTime() {
    struct tm currTime {};
    TimeService::getCurrTime(currTime);
    ESP_LOGW(TAG, ">>> CRON JOB: Current time is %d:%d", currTime.tm_hour, currTime.tm_min);
    return ESP_OK;
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Status LEDs; initialise first so every step is visible
    StatusLightManager statusLight;
    ESP_ERROR_CHECK(statusLight.init());
    esp_event_post(STATUS_LED_EVENT,
                   static_cast<int32_t>(LedState::BOOTING), nullptr, 0, 0);

    NetworkStateMachine network;

    // Mount SPIFFS for the web frontend
    esp_vfs_spiffs_conf_t spiffsConf{};
    spiffsConf.base_path              = "/spiffs";
    spiffsConf.partition_label        = "spiffs";
    spiffsConf.max_files              = 10;
    spiffsConf.format_if_mount_failed = false;
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffsConf));

    UpdateManager updateManager;
    FirmwareManager firmwareManager;
    ESP_ERROR_CHECK(firmwareManager.init());
    AppController appController{updateManager, firmwareManager};

    // Start webserver 
    web_firmware_callbacks_t fwCbs{};
    appController.fillFirmwareCallbacks(&fwCbs);

    web_network_callbacks_t netCbs{};
    network.fillNetworkCallbacks(&netCbs);

    esp_br_web_start("/spiffs", &fwCbs, &netCbs);

    ESP_LOGI(TAG, "Waiting for internet connection...");
    network.waitUntilInternetIsConnected();
    ESP_LOGI(TAG, "Routed Connection available! Starting Application...");
    TimeService::init();

    struct tm currTime {};
    TimeService::getCurrTime(currTime);
    ESP_LOGW(TAG, "Current time is %d:%d", currTime.tm_hour, currTime.tm_min);

    Cron::init();
    cron_timing_t timing{};
    Cron::scheduleJob(timing, printTime);

    // Boot-decision-tree → normal operation (may reboot and never return)
    appController.run();

    // Reached only in non-blocking modes (e.g. ZIGBEE_ROUTER)
    while (true)
    {
        vTaskDelay(portMAX_DELAY);
    }
}
