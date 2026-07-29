idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES
    PRIV_REQUIRES
        board_config
        cc_bsl
        esp_http_client
        esp_https_ota
        esp_partition
        app_update
        esp_driver_uart
        esp_driver_gpio
        mbedtls
        freertos
        log
        sse_events
)
