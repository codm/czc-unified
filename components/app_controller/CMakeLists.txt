idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES
        update_manager
        firmware_manager
        nvs_flash
        esp_system
        log
)
