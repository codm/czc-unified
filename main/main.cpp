#include "NetworkStateMachine.h"
#include "System_manager.h"
#include "nvs_flash.h"
#include "esp_event.h"

const char* TAG = "esp_ot_br";
bool wifi_connected = false;

extern "C" void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    Rcp_interface rcp_interface;
    System_manager system_manager(rcp_interface, UART_NUM_2);

    EthernetAPI ethernetAPI;
    
    WirelessAPI wirelessAPI("otbr-codm", "codmcodm", 2); // default AP 
    
    NetworkStateMachine networkStateMachine(ethernetAPI, wirelessAPI);
    networkStateMachine.initNetworkStateMachine();
    
    system_manager.initThread();

    ESP_LOGI(TAG, "Init completed!, waiting for Internet connection, %d", networkStateMachine.getState());
    // wait until network connection is setup
    while (networkStateMachine.getState() != NetworkState::WLAN &&
           networkStateMachine.getState() != NetworkState::ETHERNET) {
        vTaskDelay(100);
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // wait 1s for Wifi to connect, proper check function has to be implemented

    system_manager.flashRcpFirmwareWhenConfigured();
    // system_manager.initRcpFirmwareFlash("http://192.168.178.189:8080/ot-rcp-bsl.bin");

    ESP_LOGI(TAG, "Network connected! Starting OTBR...");
    system_manager.startThread();

    while(1) vTaskDelay(portMAX_DELAY); // Loop so main doesnt finish and objects are deleted
}
