idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    REQUIRES 
        freertos
        esp_system
        esp_event
        esp_common
        esp_hw_support
        log
        app_update
        esp_http_client
        esp_https_ota
        nvs_flash
        mbedtls
        esp_driver_uart
        spiffs
        esp_driver_gpio
        espressif__mdns

        # Thread Controller requires 
        openthread
        esp_coex 
        esp_ot_br_server 
        spiffs
        esp_wifi
        espressif__mdns
        ot_examples_br
        ot_examples_common
)
# split into priv and non priv required and check different requires it they are really needed 