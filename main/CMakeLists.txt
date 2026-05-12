idf_component_register(SRCS "main.cpp"
                       REQUIRES    
                                system_manager
                                network
                                nvs_flash
                                esp_event

                       INCLUDE_DIRS ".")
