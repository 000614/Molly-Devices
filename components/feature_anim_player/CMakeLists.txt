idf_component_register(SRCS "feature_anim_player.c"
                    INCLUDE_DIRS "."
                    REQUIRES bsp
                    PRIV_REQUIRES esp_new_jpeg) # 声明依赖关系