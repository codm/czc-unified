idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include

    PRIV_REQUIRES
        board_config
        nvs_flash
        time_service

    REQUIRES
        log
)
