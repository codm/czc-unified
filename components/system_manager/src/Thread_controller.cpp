#include "Thread_controller.h"
#include "nvs_flash.h"

const char* Thread_controller::TAG = "esp_ot_br";

void Thread_controller::rcp_failure_hardware_reset_handler()
{
    gpio_config_t reset_pin_config;
    memset(&reset_pin_config, 0, sizeof(reset_pin_config));
    reset_pin_config.intr_type = GPIO_INTR_DISABLE;
    reset_pin_config.pin_bit_mask = BIT(PIN_TO_RCP_RESET);
    reset_pin_config.mode = GPIO_MODE_OUTPUT;
    reset_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    reset_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&reset_pin_config);
    gpio_set_level(PIN_TO_RCP_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TO_RCP_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_reset_pin(PIN_TO_RCP_RESET);
}

esp_err_t Thread_controller::init()
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
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp-ot-br"));

    // configure webserver start on ETH / STA GOT IP Events
    web_server_conf = {
        .base_path = "/spiffs", .partition_label = "spiffs", .max_files = 10, .format_if_mount_failed = false};
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&web_server_conf));
    esp_br_web_start("/spiffs"); 

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
    ESP_ERROR_CHECK(esp_openthread_start(&config));
    // ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
    ot_network_auto_start();  
    ESP_LOGI(TAG, "OTBR started!");
    
    thread_active = true;
    return ESP_OK;
}

esp_err_t Thread_controller::stop()
{
    thread_active = false;
    return esp_err_t();
}

bool Thread_controller::is_running()
{
    return thread_active;
}
