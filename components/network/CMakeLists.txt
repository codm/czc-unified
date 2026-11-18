idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    PRIV_REQUIRES
        board_config
        espressif__lan87xx
        log

    REQUIRES
        esp_eth 
        esp_netif 
        esp_event
)
