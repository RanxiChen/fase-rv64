project(FaseRV64)

cmake_minimum_required(VERSION 3.00)

if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)

include_directories(
        src
        )

file(GLOB_RECURSE ALL_CXX_SRC
        src/*.cpp
        )

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/conf/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/conf/ FILES_MATCHING PATTERN "*.ini")

add_executable(fase-rv64 ${ALL_CXX_SRC})

add_subdirectory(example)
