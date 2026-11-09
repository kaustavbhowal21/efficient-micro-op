cmake_minimum_required(VERSION 3.15)
project(efficient_micro_op CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB SOURCES 
    "src/*.cc"
)

add_executable(uop_sim ${SOURCES})

if (MSVC)
    target_compile_options(uop_sim PRIVATE /W4 /O2)
else()
    target_compile_options(uop_sim PRIVATE -Wall -Wextra -O3)
endif()

