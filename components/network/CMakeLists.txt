idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_REQUIRES
        board_config
        espressif__lan87xx
        log

    REQUIRES
        esp_eth
        esp_netif
        esp_event
        esp_wifi
        esp_timer
        nvs_flash
)
