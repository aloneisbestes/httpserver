# 设置所有源文件
set(ALL_SRC common.cpp log.cpp main.cpp)

# 导入头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 设置选择是否定义宏, 默认是定义
option(T_DEBUG "Whether to define debug macros?" ON)
if (T_DEBUG)
    add_definitions(-DDEBUG)
endif (T_DEBUG)

# 消除警告
add_definitions(-w)

# 生成可执行文件
add_executable(httpserver ${ALL_SRC})

target_link_libraries(httpserver pthread)