idf_component_register(SRCS "bsp_button.c" # <-- 添加 "bsp_button.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_lcd)