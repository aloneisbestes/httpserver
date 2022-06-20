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

# 添加可使用 gdb 调试
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread")

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

# testHttp
add_executable(testHttp testHttp.cpp ${NEED_SRC} ../src/http.cpp)

# 连接库
target_link_libraries(testMysqlPool mysqlclient)
target_link_libraries(testMysqlPool pthread)
target_link_libraries(testComm pthread)
target_link_libraries(testComm mysqlclient)
target_link_libraries(test pthread)
target_link_libraries(test mysqlclient)
target_link_libraries(testExp pthread)
target_link_libraries(testExp mysqlclient)
target_link_libraries(testHttp pthread)
target_link_libraries(testHttp mysqlclient)