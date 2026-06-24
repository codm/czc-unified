idf_component_register(SRCS "main.cpp"
                       REQUIRES
                                network
                                status_light
                                update_manager
                                firmware_manager
                                app_controller
                                esp_ot_br_server
                                spiffs
                                nvs_flash
                                esp_event
                                log

                       INCLUDE_DIRS ".")
