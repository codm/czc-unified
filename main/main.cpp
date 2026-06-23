#include "NetworkStateMachine.h"
#include "System_manager.h"
#include "nvs_flash.h"
#include "esp_event.h"

const char* TAG = "esp_ot_br";

extern "C" void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    Rcp_interface rcp_interface;
    System_manager system_manager(rcp_interface, UART_NUM_2);

    NetworkStateMachine networkStateMachine;
    
    if (!system_manager.isRcpFlashPending()) {
        system_manager.initThread();
    }

    ESP_LOGI(TAG, "Init completed!, waiting for Internet connection");
    networkStateMachine.waitUntilInternetIsConnected();

    if (system_manager.isDeviceSetup() == false) {
        char* rcp_v1_url = "https://github.com/codm/czc-ot-rcp-fw/releases/download/V1.0.0/czc_ot_rcp_fw_1.0.0.bin";
        system_manager.writeDeviceSetup(true);
        system_manager.initRcpFirmwareFlash(rcp_v1_url);
    }

    if (system_manager.isRcpFlashPending()) {
        system_manager.flashRcpFirmwareWhenConfigured();
    } else {
        ESP_LOGI(TAG, "Network connected! Starting OTBR...");
        system_manager.startThread();
    }

    while(1) vTaskDelay(portMAX_DELAY); // Loop so main doesnt finish and objects are deleted
}
