idf_component_register(SRCS "main.cpp"
                            "net_web_bridge.cpp"
                       REQUIRES
                                network
                                status_light
                                update_manager
                                firmware_manager
                                app_controller
                                esp_ot_br_server
                                time_service
                                cron
                                sse_events
                                board_config

                                button_handler
                                spiffs
                                nvs_flash
                                esp_event
                                esp_netif
                                espressif__mdns
                                log

                       INCLUDE_DIRS ".")
