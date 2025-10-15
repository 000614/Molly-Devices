# 注册 main 组件
idf_component_register(SRCS "app_main.c"
                      INCLUDE_DIRS "."
                      REQUIRES feature_anim_player 
                               button 
                               app_logic 
                               storage_manager
                               fatfs
                               
                               )
# spiffs_create_partition_image(storage storage FLASH_IN_PROJECT)
# # 使用原生 CMake 命令创建 storage.bin 镜像
# if(NOT CMAKE_BUILD_EARLY_EXPANSION)
#     set(MKFATFS "${IDF_PATH}/components/fatfs/fatfsgen.py")
#     set(STORAGE_DATA_DIR "${PROJECT_DIR}/storage_data")
#     set(STORAGE_IMAGE "${CMAKE_BINARY_DIR}/storage.bin")
    
#     # 检查 Python 脚本是否存在
#     if(EXISTS "${MKFATFS}")
#         add_custom_command(
#             OUTPUT ${STORAGE_IMAGE}
#             COMMAND ${Python3_EXECUTABLE} ${MKFATFS}
#                     ${STORAGE_DATA_DIR}
#                     --partition_size 0x7c0000  # 7936KB
#                     --output_file ${STORAGE_IMAGE}
#             DEPENDS ${STORAGE_DATA_DIR}
#             COMMENT "生成 storage 分区镜像..."
#             VERBATIM
#         )
        
#         add_custom_target(storage_image ALL DEPENDS ${STORAGE_IMAGE})
#         add_dependencies(${COMPONENT_LIB} storage_image)
        
#         # 添加到烧录命令
#         esptool_py_flash_target_image(flash storage 0xA10000 ${STORAGE_IMAGE})
#     else()
#         message(WARNING "未找到 fatfsgen.py，跳过 storage 镜像生成")
#     endif()
# endif()