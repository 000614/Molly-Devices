# 1. 使用 file(GLOB ...) 自动查找当前目录下的所有 .c 和 .cpp 源文件
#    "component_sources" 是我们自定义的变量名，用来存储找到的文件列表
file(GLOB component_sources "*.c" "*.cpp")

# 2. 注册组件，将上面找到的文件列表变量传递给 SRCS 参数
idf_component_register(SRCS ${component_sources}
                       INCLUDE_DIRS "include"
                       REQUIRES     esp_lcd
                                    driver
                       # ... 其他依赖项
                      )