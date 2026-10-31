idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    PRIV_REQUIRES
        board_config
        esp_driver_gpio
        cron
        nvs_flash

    REQUIRES
        esp_event
        esp_timer
        log
)
