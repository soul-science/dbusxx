cmake_minimum_required(VERSION 3.15)
project(example_install
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dbusxx REQUIRED)

add_executable(example_install
    main.cpp
)

target_link_libraries(example_install
    PRIVATE
        dbusxx
)
