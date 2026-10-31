idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include

    PRIV_REQUIRES
        board_config
        lwip
        esp_netif
        nvs_flash

    REQUIRES
        log
)
