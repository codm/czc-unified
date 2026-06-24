#include "NetworkStateMachine.h"
#include "status_light_manager.h"
#include "status_light_event.h"
#include "update_manager.h"
#include "firmware_manager.h"
#include "app_controller.h"
#include "esp_br_web.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Status LEDs — initialise first so every step is visible
    StatusLightManager statusLight;
    ESP_ERROR_CHECK(statusLight.init());
    esp_event_post(STATUS_LED_EVENT,
                   static_cast<int32_t>(LedState::BOOTING), nullptr, 0, 0);

    // Network — starts in background (Ethernet + optionally WiFi)
    NetworkStateMachine network;

    // Mount SPIFFS for the web frontend
    esp_vfs_spiffs_conf_t spiffsConf{};
    spiffsConf.base_path              = "/spiffs";
    spiffsConf.partition_label        = "spiffs";
    spiffsConf.max_files              = 10;
    spiffsConf.format_if_mount_failed = false;
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffsConf));

    // Managers — owned here, injected by reference into AppController
    UpdateManager   updateManager;
    FirmwareManager firmwareManager;
    AppController   appController{updateManager, firmwareManager};

    // Start web server early — user may need to configure WiFi through it
    // before any internet connection is available (AP mode / first boot)
    web_firmware_callbacks_t fwCbs{};
    appController.fillFirmwareCallbacks(&fwCbs);

    web_network_callbacks_t netCbs{};
    network.fillNetworkCallbacks(&netCbs);

    esp_br_web_start("/spiffs", &fwCbs, &netCbs);

    // Now wait for a routed connection (Ethernet or WiFi)
    // If no WiFi is configured the NSM opens an AP — user configures
    // WiFi through the web UI and NSM reconnects automatically
    ESP_LOGI(TAG, "Waiting for internet connection...");
    network.waitUntilInternetIsConnected();

    // Boot-decision-tree → normal operation (may reboot and never return)
    appController.run();

    // Reached only in non-blocking modes (e.g. ZIGBEE_ROUTER)
    while (true)
    {
        vTaskDelay(portMAX_DELAY);
    }
}
