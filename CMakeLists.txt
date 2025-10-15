# The following lines of boilerplate have to be in your project's
# top-level CMakeLists.txt file.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义你的项目名称
project(molly-devices)

# # 注册你的主应用程序组件 (main)
# # 这是关键的修改步骤：我们添加了 PRIV_REQUIRES fatfs
# # 这告诉编译系统，主组件在构建时需要 fatfs 组件的功能
# idf_component_register(
#     SRCS "main/app_main.c"
#     INCLUDE_DIRS "main"
#     PRIV_REQUIRES fatfs  # <-- *** 添加这一行来解决问题 ***
# )

# # 这个命令会找到你项目根目录下的 'storage' 文件夹，
# # 将其内容打包成一个FAT文件系统镜像，并与名为 'storage' 的分区关联起来。
# idf_fatfs_create_flash_image(
#     storage   # 分区名 (必须与 partitions.csv 中的名字匹配)
#     storage   # 包含源文件的目录名
#     FLASH_IN_PROJECT # 这个标志确保 `idf.py flash` 会自动烧录这个镜像
# )