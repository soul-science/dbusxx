# CMake version >= 3.15
cmake_minimum_required(VERSION 3.15)

# C++ version >= 17
set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

project(ssdbus VERSION 1.0.0 LANGUAGES CXX)

# library
add_subdirectory(library)

# example
add_subdirectory(example)