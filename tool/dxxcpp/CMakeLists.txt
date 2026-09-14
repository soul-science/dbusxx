# CMake version >= 3.15
cmake_minimum_required(VERSION 3.15)

project(dxxcpp)

# C++ version >= 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dxxcpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Sema.cpp
    src/Codegen.cpp
    main.cpp
)

set(
    DXXCPP_INCLUDE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(dxxcpp
    PRIVATE
        ${DXXCPP_INCLUDE_DIR}
)