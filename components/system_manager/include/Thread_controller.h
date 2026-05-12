
#pragma once
#include "esp_coexist.h"
#include "esp_openthread_types.h"
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_spinel.h"
#include "esp_openthread_types.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_eventfd.h"
#include "mdns.h"
extern "C" {
#include "ot_examples_br.h"
#include "ot_examples_common.h"
}

#include "esp_br_web.h"
#include "esp_spiffs.h"

// #include "ot_led_strip.h" // include it in cmakelist ot_led_strip

#define PIN_TO_RCP_RESET GPIO_NUM_16

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()              \
    {                                                      \
        .radio_mode = RADIO_MODE_UART_RCP,                 \
        .radio_uart_config = {                             \
            .port = UART_NUM_2,                                     \
            .uart_config =                                 \
                {                                          \
                    .baud_rate = 921600,                   \
                    .data_bits = UART_DATA_8_BITS,         \
                    .parity = UART_PARITY_DISABLE,         \
                    .stop_bits = UART_STOP_BITS_1,         \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
                    .rx_flow_ctrl_thresh = 0,              \
                    .source_clk = UART_SCLK_DEFAULT,       \
                },                                         \
            .rx_pin = GPIO_NUM_36,                                   \
            .tx_pin = GPIO_NUM_4,                                   \
        },                                                 \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                        \
    {                                                               \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,          \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()    \
    {                                           \
        .storage_partition_name = "nvs",        \
        .netif_queue_size = 10,                 \
        .task_queue_size = 10,                  \
    }

class Thread_controller 
{
private: 
    static const char* TAG;
    static void rcp_failure_hardware_reset_handler();
    bool thread_active = false;

    esp_vfs_eventfd_config_t eventfd_config;
    esp_vfs_spiffs_conf_t web_server_conf;
    esp_openthread_config_t config;

public:
    esp_err_t init();
    esp_err_t start();
    esp_err_t stop();
    bool is_running();
};