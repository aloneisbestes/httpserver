# 定义至少需要的版本号
cmake_minimum_required(VERSION 3.0)

# 项目名
project(httpserver)

# 引用其它目录的 CMakeLists
# 选择使用 test 测试还是使用 src 下面的 main

# option 用来确定是否使用 test 目录用来测试，ON: 表示使用，OFF: 表示不使用
option(ISTEST "Whether to use the test directory?" OFF)

if (ISTEST) 
    # 该分支表示使用 test 目录用来测试
    add_subdirectory(test ${PROJECT_SOURCE_DIR}/test/bin)
else ()
    add_subdirectory(src ${PROJECT_SOURCE_DIR}/bin)
endif (ISTEST)