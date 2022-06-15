# 导入头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 设置需要的源文件
set(NEED_SRC ../src/log.cpp ../src/common.cpp ../src/mysqlpool.cpp)

# 设置选择是否定义宏, 默认是定义
option(T_DEBUG "Whether to define debug macros?" ON)
if (T_DEBUG)
    add_definitions(-DDEBUG)
endif (T_DEBUG)


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/test/bin)

####
# 生成可执行文件
####
# testExp
add_executable(testExp testExp.cpp ${NEED_SRC})

# test
add_executable(test test.cpp ${NEED_SRC})

# testCommon
add_executable(testComm testCommon.cpp ${NEED_SRC})

# testMysqlPool
add_executable(testMysqlPool testMysqlPool.cpp ${NEED_SRC})

# 连接库
target_link_libraries(testMysqlPool mysqlclient)
target_link_libraries(testMysqlPool pthread)
target_link_libraries(testComm pthread)
target_link_libraries(testComm mysqlclient)
target_link_libraries(test pthread)
target_link_libraries(test mysqlclient)
target_link_libraries(testExp pthread)
target_link_libraries(testExp mysqlclient)