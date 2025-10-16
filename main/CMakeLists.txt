# 注册 main 组件
idf_component_register(SRCS "app_main.c"
                      INCLUDE_DIRS "."
                      REQUIRES feature_anim_player 
                               feature_button 
                               app_logic 
                               storage_manager
                               fatfs
                               
                               )
