cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)

project (EntryTaskObjectStorage)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
)

add_subdirectory(src)
