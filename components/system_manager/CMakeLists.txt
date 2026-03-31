idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    REQUIRES 
        freertos
        esp_system
        esp_event
        esp_common
        esp_hw_support
        log
        app_update
        esp_http_client
        esp_https_ota
        nvs_flash
)
# split into priv and non priv requires