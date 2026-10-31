idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_REQUIRES
        board_config
        esp_event
        esp_timer
        log
        firmware_manager
        status_light

    REQUIRES
        esp_driver_gpio
        button
        app_controller
)
