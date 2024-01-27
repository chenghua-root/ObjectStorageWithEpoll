# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量
aux_source_directory(. DIR_LIB_SRCS)

add_definitions(-D_GNU_SOURCE) # CPU_SET

# 生成链接库
add_library(lib STATIC ${DIR_LIB_SRCS})

target_link_libraries(lib PUBLIC
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(lib PUBLIC Threads::Threads)
