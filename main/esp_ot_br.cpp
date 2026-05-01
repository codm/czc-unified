/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Border Router Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

extern "C" {
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_coexist.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_spinel.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_eventfd.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "ot_examples_br.h"
#include "ot_examples_common.h"

#include "esp_br_web.h"
#include "esp_spiffs.h"

#include "esp_wifi.h"

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
#include "ot_led_strip.h"
#endif

#include "NetworkStateMachine.h"
}

const char* TAG = "esp_ot_br";
bool wifi_connected = false;

#include "System_manager.h"

#define PIN_TO_RCP_RESET CONFIG_OPENTHREAD_HW_RESET_RCP_PIN
static void rcp_failure_hardware_reset_handler(void)
{
    gpio_config_t reset_pin_config;
    memset(&reset_pin_config, 0, sizeof(reset_pin_config));
    reset_pin_config.intr_type = GPIO_INTR_DISABLE;
    reset_pin_config.pin_bit_mask = BIT(GPIO_NUM_16);
    reset_pin_config.mode = GPIO_MODE_OUTPUT;
    reset_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    reset_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&reset_pin_config);
    gpio_set_level(GPIO_NUM_16, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_NUM_16, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_reset_pin(GPIO_NUM_16);
}

extern "C" void app_main(void) {
    // setup
    
    // Used eventfds:
    // * netif
    // * task queue
    // * border router
    size_t max_eventfd = 3;

    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = max_eventfd,
    };

    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp-ot-br"));

    Rcp_interface rcp_interface;
    System_manager system_manager(rcp_interface, UART_NUM_2); // uart num from OTBR uart config

    EthernetAPI ethernetAPI;
    
    WirelessAPI wirelessAPI("otbr-codm", "codmcodm", 2); // default AP 
    
    NetworkStateMachine networkStateMachine(ethernetAPI, wirelessAPI);
    networkStateMachine.initNetworkStateMachine();
    ESP_LOGI(TAG, "Init completed!, waiting for Internet connection, %d", networkStateMachine.getState());

    // wait until network connection is setup
    while (networkStateMachine.getState() != NetworkState::WLAN &&
           networkStateMachine.getState() != NetworkState::ETHERNET) {
        vTaskDelay(100);
    }

    ESP_LOGI(TAG, "Internet Connected, starting Webserver ...");
    // start webserver
    esp_vfs_spiffs_conf_t web_server_conf = {
        .base_path = "/spiffs", .partition_label = "spiffs", .max_files = 10, .format_if_mount_failed = false};
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&web_server_conf));
    esp_br_web_start("/spiffs"); 

    ESP_LOGI(TAG, "Network connected! Startin OTBR...");
    // start OTBR
    esp_openthread_register_rcp_failure_handler(rcp_failure_hardware_reset_handler);

    static esp_openthread_config_t config = {
        .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
        .platform_config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        },
    };

    ESP_ERROR_CHECK(esp_openthread_start(&config));

    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
    ot_network_auto_start();  
    ESP_LOGI(TAG, "OTBR started, main() finished");
}

