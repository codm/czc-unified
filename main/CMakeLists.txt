idf_component_register(SRCS "esp_ot_br.cpp"
                       REQUIRES esp_psram openthread esp_coex esp_wifi nvs_flash esp_ot_br_server spiffs esp_event system_manager
                       INCLUDE_DIRS ".")
