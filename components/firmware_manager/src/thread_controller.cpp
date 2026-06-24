#include "thread_controller.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "openthread/dataset.h"
#include <string.h>

static const char* TAG = "ThreadController";

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                   \
    {                                                           \
        .radio_mode = RADIO_MODE_UART_RCP,                      \
        .radio_uart_config = {                                  \
            .port = Board::RCP_UART,                            \
            .uart_config =                                      \
                {                                               \
                    .baud_rate = Board::SPINEL_BAUD,            \
                    .data_bits = UART_DATA_8_BITS,              \
                    .parity    = UART_PARITY_DISABLE,           \
                    .stop_bits = UART_STOP_BITS_1,              \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      \
                    .rx_flow_ctrl_thresh = 0,                   \
                    .source_clk = UART_SCLK_DEFAULT,            \
                },                                              \
            .rx_pin = Board::RCP_UART_RX,                       \
            .tx_pin = Board::RCP_UART_TX,                       \
        },                                                      \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()            \
    {                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()    \
    {                                           \
        .storage_partition_name = "nvs",        \
        .netif_queue_size = 10,                 \
        .task_queue_size  = 10,                 \
    }

ThreadController::ThreadController()
    : threadActive{false}
{}

esp_err_t ThreadController::start()
{
    esp_vfs_eventfd_config_t eventfdCfg{};
    eventfdCfg.max_fds = 3;  // netif + task queue + border router
    ESP_RETURN_ON_ERROR(esp_vfs_eventfd_register(&eventfdCfg),
                        TAG, "eventfd register failed");

    esp_openthread_register_rcp_failure_handler(rcpFailureHandler);

    esp_openthread_config_t config{};
    config.netif_config    = ESP_NETIF_DEFAULT_OPENTHREAD();
    config.platform_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    esp_netif_t* backboneNetif{esp_netif_get_handle_from_ifkey("ETH_DEF")};
    if (backboneNetif == nullptr) {
        ESP_LOGW(TAG, "ETH_DEF not found, trying WIFI_STA_DEF");
        backboneNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    if (backboneNetif != nullptr) {
        esp_openthread_set_backbone_netif(backboneNetif);
    } else {
        ESP_LOGE(TAG, "No backbone netif found — border routing will not work");
    }

    ESP_RETURN_ON_ERROR(esp_openthread_start(&config), TAG, "openthread start failed");

    // border_router_init requires the OT task mutex to be held
    esp_openthread_lock_acquire(portMAX_DELAY);
    ESP_ERROR_CHECK(esp_openthread_border_router_init());
    esp_openthread_lock_release();

    otOperationalDatasetTlvs dataset{};
    esp_openthread_lock_acquire(portMAX_DELAY);
    otError otErr{otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset)};
    ESP_ERROR_CHECK(esp_openthread_auto_start((otErr == OT_ERROR_NONE) ? &dataset : nullptr));
    esp_openthread_lock_release();

    threadActive = true;
    ESP_LOGI(TAG, "OTBR started");
    return ESP_OK;
}

esp_err_t ThreadController::stop()
{
    esp_openthread_register_rcp_failure_handler(nullptr);

    otInstance* ins{esp_openthread_get_instance()};
    ESP_ERROR_CHECK(otThreadDetachGracefully(ins, nullptr, nullptr));

    esp_openthread_lock_acquire(portMAX_DELAY);
    ESP_ERROR_CHECK(otIp6SetEnabled(ins, false));
    esp_openthread_lock_release();

    esp_err_t ret{esp_openthread_stop()};
    threadActive = false;
    return ret;
}

bool ThreadController::isRunning()
{
    return threadActive;
}

void ThreadController::rcpFailureHandler()
{
    ESP_LOGW(TAG, "RCP failure — hardware reset");

    gpio_config_t cfg{};
    cfg.pin_bit_mask = (1ULL << Board::CC_RST_PIN);
    cfg.mode         = GPIO_MODE_OUTPUT;
    gpio_config(&cfg);

    gpio_set_level(Board::CC_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(Board::CC_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_reset_pin(Board::CC_RST_PIN);
}
