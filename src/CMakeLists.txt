set(CMAKE_C_FLAGS "-std=gnu99")

message("*************************build type " ${CMAKE_BUILD_TYPE} "*************************")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -ggdb -DDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -ggdb -DDEBUG")
    set(CMAKE_*_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -ggdb -DDEBUG") # 链接文件编译时也支持gdb
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -finline-limit=10000 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O2 -finline-limit=10000 -DNDEBUG")
endif()

add_compile_definitions (LOG_USE_COLOR)

add_subdirectory (lib)
add_subdirectory (s3client)
add_subdirectory (s3server)
