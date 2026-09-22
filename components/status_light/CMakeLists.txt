idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_REQUIRES
        board_config
        esp_driver_gpio
        cron

    REQUIRES
        esp_event
        esp_timer
        log
)
