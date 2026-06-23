idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include

    REQUIRES
        esp_netif
        esp_eth
        esp_wifi
        esp_timer
        esp_ot_br_server

    PRIV_REQUIRES
        nvs_flash
        espressif__mdns
)
