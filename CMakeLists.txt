
# DEBUG
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# RELEASE
# cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release && cmake --build build_release

# CMake version >= 3.15
cmake_minimum_required(VERSION 3.15)

# C++ version >= 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(dbusxx
    VERSION 1.0.0
    LANGUAGES CXX
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

# unit tests (Google Test; spawns its own private dbus-daemon, so no system
# D-Bus daemon is required — covers pure-logic layers + business-layer E2E)
option(DBUSXX_BUILD_TESTS "Build unit tests" ON)
if(DBUSXX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit_tests)
endif()