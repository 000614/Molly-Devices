idf_component_register(SRCS "feature_motor.c"
                    INCLUDE_DIRS "."
                    REQUIRES bsp event_manager
                    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc freertos
                    )