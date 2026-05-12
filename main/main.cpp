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
    System_manager system_manager(rcp_interface, UART_NUM_2); // uart num from OTBR uart config

    EthernetAPI ethernetAPI;
    
    WirelessAPI wirelessAPI("otbr-codm", "codmcodm", 2); // default AP 
    
    NetworkStateMachine networkStateMachine(ethernetAPI, wirelessAPI);
    networkStateMachine.initNetworkStateMachine();
    
    Thread_controller thread_controller;
    thread_controller.init(); 
    ESP_LOGI(TAG, "Init completed!, waiting for Internet connection, %d", networkStateMachine.getState());
    // wait until network connection is setup
    while (networkStateMachine.getState() != NetworkState::WLAN &&
           networkStateMachine.getState() != NetworkState::ETHERNET) {
        vTaskDelay(100);
    }
    ESP_LOGI(TAG, "Network connected! Startin OTBR...");
    // start OTBR

    // ESP_LOGI(TAG, "Flashing RCP firmware");
    // const char* rcp_url = "http://192.168.178.189:8080/ot-rcp.ihex";
    // system_manager.flashRcpFirmware(rcp_url);

    thread_controller.start();
    while(1) vTaskDelay(portMAX_DELAY); // Loop so main doesnt finish and objects are deleted
}

