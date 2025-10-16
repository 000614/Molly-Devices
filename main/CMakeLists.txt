# 自动发现components目录下的所有组件
# 1. 设置components目录的路径
set(COMPONENTS_DIR ${CMAKE_SOURCE_DIR}/components)

# 2. 使用 file(GLOB ...) 获取所有子目录的完整路径
file(GLOB component_paths LIST_DIRECTORIES true "${COMPONENTS_DIR}/*")

set(component_names "")
# 3. 循环处理每个路径，提取目录名作为组件名
foreach(path ${component_paths})
    if(IS_DIRECTORY ${path})
        # 从完整路径中获取最后的目录名
        get_filename_component(name ${path} NAME)
        # 将组件名添加到列表中
        list(APPEND component_names ${name})
    endif()
endforeach()

# 4. 注册main组件，并将前面获取的所有组件名作为依赖项
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       # 使用 PRIV_REQUIRES 或 REQUIRES
                       # PRIV_REQUIRES: 依赖是私有的，不会传递给依赖main的组件
                       # REQUIRES: 依赖是公开的，会传递
                       PRIV_REQUIRES ${component_names})