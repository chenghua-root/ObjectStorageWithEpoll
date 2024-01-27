# 查找当前目录下的所有源文件
aux_source_directory(. SRC_LIST)

# 指定include路径
link_directories (
    ${CMAKE_BINARY_DIR}/src
)

# 指定生成目标
add_executable(s3server ${SRC_LIST})

# 添加链接库
target_link_libraries(s3server
    lib
    -lleveldb
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(s3server Threads::Threads)

# 指定生成路径
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
