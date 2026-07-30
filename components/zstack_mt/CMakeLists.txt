idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES
        esp_driver_uart 
    PRIV_REQUIRES
        board_config
        esp_driver_gpio
        log
)
