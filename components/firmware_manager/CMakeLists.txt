idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES
        board_config
        esp_event
        esp_netif
        openthread
        esp_coex
        esp_driver_gpio
        nvs_flash
        freertos
        log
        spiffs
        status_light

    PRIV_REQUIRES
        esp_driver_uart
        lwip
        zstack_mt
)
