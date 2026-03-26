idf_component_register(SRCS "esp_ot_br.c"
                       PRIV_REQUIRES esp_psram openthread esp_coex esp_wifi nvs_flash esp_ot_br_server spiffs esp_event
                       INCLUDE_DIRS ".")
