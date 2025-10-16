# 1. 定义components目录的路径
#    ${CMAKE_CURRENT_LIST_DIR} 是当前CMakeLists.txt文件所在的目录
#    所以 ${CMAKE_CURRENT_LIST_DIR}/.. 就是components目录
set(COMPONENTS_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

# 2. 获取当前组件的名称
#    从当前目录路径中提取最后的目录名
get_filename_component(CURRENT_COMPONENT_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

# 3. 使用 file(GLOB ...) 发现所有兄弟组件的目录
file(GLOB sibling_component_paths LIST_DIRECTORIES true "${COMPONENTS_DIR}/*")

set(sibling_component_names "")
# 4. 循环处理，提取组件名并排除自身
foreach(path ${sibling_component_paths})
    if(IS_DIRECTORY ${path})
        get_filename_component(name ${path} NAME)
        # 检查提取出的组件名是否是当前组件自己
        if(NOT name STREQUAL CURRENT_COMPONENT_NAME)
            list(APPEND sibling_component_names ${name})
        endif()
    endif()
endforeach()

# 5. 注册 app_logic 组件，并自动包含所有其他兄弟组件作为依赖
idf_component_register(SRCS "app_logic.c"
                       INCLUDE_DIRS "."
                       # 根据需要使用 REQUIRES 或 PRIV_REQUIRES
                       PRIV_REQUIRES ${sibling_component_names}
)