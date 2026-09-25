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

# Semantic layer regression validation (positive and negative examples):
# run manually or with: ctest -R dxxcpp_verify
option(DXXCPP_BUILD_TESTS "Build dxxcpp sema verify tests" ON)

if(DXXCPP_BUILD_TESTS)
    # Test registration lives next door: the CLI cases alone need fixtures,
    # exit-code wrappers, a fuzz run and a generated-code compile check
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- install the generator ----
# Consumers look for it next to the library (<prefix>/bin) via find_program;
# the headers it emits are tied to the matching library version
include(GNUInstallDirs)

install(TARGETS dxxcpp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})