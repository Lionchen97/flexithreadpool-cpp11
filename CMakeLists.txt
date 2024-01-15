cmake_minimum_required(VERSION 2.8)
project(threadpool)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(main src/main.cpp) 

# 添加头文件搜索路径
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# 生成动态库
add_library(tdpool SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/threadpool.cpp)
target_include_directories(tdpool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# 设置库的输出路径
set_target_properties(tdpool PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

# 查找并链接线程库
find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE ${CMAKE_THREAD_LIBS_INIT} tdpool)

# 添加编译选项
if(MSVC)
    # 对于 MSVC 编译器
    target_compile_options(main PRIVATE /W4)
else()
    # 对于 GCC 或 Clang
    target_compile_options(main PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加链接选项
if(UNIX)
    target_link_libraries(main PRIVATE -pthread)
endif()
