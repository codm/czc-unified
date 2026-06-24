#include "Thread_controller.h"

#include "board_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_coexist.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_border_router.h"
#include "esp_vfs_dev.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "openthread/dataset.h"
#include <string.h>

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                   \
    {                                                           \
        .radio_mode = RADIO_MODE_UART_RCP,                      \
        .radio_uart_config = {                                  \
            .port = Board::RCP_UART,                            \
            .uart_config =                                      \
                {                                               \
                    .baud_rate = Board::SPINEL_BAUD,            \
                    .data_bits = UART_DATA_8_BITS,              \
                    .parity = UART_PARITY_DISABLE,              \
                    .stop_bits = UART_STOP_BITS_1,              \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      \
                    .rx_flow_ctrl_thresh = 0,                   \
                    .source_clk = UART_SCLK_DEFAULT,            \
                },                                              \
            .rx_pin = Board::RCP_UART_RX,                       \
            .tx_pin = Board::RCP_UART_TX,                       \
        },                                                      \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()           \
    {                                                  \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()   \
    {                                          \
        .storage_partition_name = "nvs",       \
        .netif_queue_size = 10,                \
        .task_queue_size = 10,                 \
    }

const char* Thread_controller::TAG = "esp_ot_br";

void Thread_controller::rcp_failure_hardware_reset_handler()
{
    ESP_LOGW(TAG, "RCP Reset handler called");
    gpio_config_t reset_pin_config;
    memset(&reset_pin_config, 0, sizeof(reset_pin_config));
    reset_pin_config.intr_type = GPIO_INTR_DISABLE;
    reset_pin_config.pin_bit_mask = BIT(Board::CC_RST_PIN);
    reset_pin_config.mode = GPIO_MODE_OUTPUT;
    reset_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    reset_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&reset_pin_config);
    gpio_set_level(Board::CC_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(Board::CC_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_reset_pin(Board::CC_RST_PIN);
}

esp_err_t Thread_controller::init(const system_flash_callbacks_t *flash_cbs)
{
    // setup
    
    // Used eventfds:
    // * netif
    // * task queue
    // * border router
    size_t max_eventfd = 3;

    eventfd_config = {
        .max_fds = max_eventfd,
    };

    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    // configure webserver start on ETH / STA GOT IP Events
    web_server_conf = {
        .base_path = "/spiffs", .partition_label = "spiffs", .max_files = 10, .format_if_mount_failed = false};
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&web_server_conf));
    esp_br_web_start("/spiffs", flash_cbs);

    esp_openthread_register_rcp_failure_handler(rcp_failure_hardware_reset_handler);
    
    this->config = {
        .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
        .platform_config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        },
    };

    return ESP_OK;
}

esp_err_t Thread_controller::start()
{
    ESP_LOGI(TAG, "Start init");

    // Backbone netif must be set before esp_openthread_start so the border
    // router can advertise OMR prefixes via RA and forward packets between
    // the Thread mesh and the home network.
    esp_netif_t *backbone_if = esp_netif_get_handle_from_ifkey("ETH_DEF");
    if (backbone_if == nullptr) {
        ESP_LOGW(TAG, "ETH_DEF netif not found, trying WIFI_STA_DEF");
        backbone_if = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    if (backbone_if != nullptr) {
        esp_openthread_set_backbone_netif(backbone_if);
    } else {
        ESP_LOGE(TAG, "No backbone netif found — border routing will not work!");
    }

    ESP_ERROR_CHECK(esp_openthread_start(&config));

    // border_router_init internally calls esp_openthread_task_switching_lock_release,
    // which asserts that the calling task holds s_openthread_task_mutex.
    // esp_openthread_lock_acquire acquires both mutexes, satisfying that requirement.
    esp_openthread_lock_acquire(portMAX_DELAY);
    ESP_ERROR_CHECK(esp_openthread_border_router_init());
    esp_openthread_lock_release();

    ESP_LOGI(TAG, "Start auto start");
    otOperationalDatasetTlvs dataset;
    esp_openthread_lock_acquire(portMAX_DELAY);
    otError ot_err = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    ESP_ERROR_CHECK(esp_openthread_auto_start((ot_err == OT_ERROR_NONE) ? &dataset : NULL));
    esp_openthread_lock_release();
    ESP_LOGI(TAG, "OTBR started!");
    
    thread_active = true;
    return ESP_OK;
}

esp_err_t Thread_controller::stop()
{
    esp_openthread_register_rcp_failure_handler(nullptr);

    otInstance *ins = esp_openthread_get_instance();
    ESP_LOGD(TAG, "Exiting OT mainloop for RCP flash...");

    // otThreadDetachGracefully with nullptr callback: OT task processes it
    // concurrently and role is DISABLED before this returns — no lock needed.
    ESP_ERROR_CHECK(otThreadDetachGracefully(ins, nullptr, nullptr));
    ESP_LOGD(TAG, "thread stop");

    // otIp6SetEnabled triggers internal task-switching-lock acquire/release.
    // Calling it without the full OT lock causes the release to be skipped
    // (wrong-task check in our esp_openthread_lock.c patch) → s_openthread_task_mutex
    // stays held → esp_openthread_stop() deadlocks on lock acquire.
    esp_openthread_lock_acquire(portMAX_DELAY);
    // ESP_ERROR_CHECK(otThreadSetEnabled(ins, false));
    ESP_ERROR_CHECK(otIp6SetEnabled(ins, false));
    esp_openthread_lock_release();
    ESP_LOGD(TAG, "ifconfig down");

    ESP_LOGD(TAG, "Stopping OT stack...");
    esp_err_t ret = esp_openthread_stop();
    thread_active = false;
    return ret;
}

bool Thread_controller::is_running()
{
    return thread_active;
}
