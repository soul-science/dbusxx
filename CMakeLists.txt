
# DEBUG
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# RELEASE
# cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release

# CMake version >= 3.15
cmake_minimum_required(VERSION 3.15)

# C++ version >= 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(dbusxx
    VERSION
        1.0.0
    LANGUAGES
        CXX
)

add_library(dbusxx_opts INTERFACE) 

target_compile_features(dbusxx_opts
    INTERFACE
        cxx_std_17
)

target_compile_options(dbusxx_opts
    INTERFACE
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-O3 -DNDEBUG -flto -march=native>
)

target_link_options(dbusxx_opts
    INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# library
add_subdirectory(library)

# example
add_subdirectory(example)