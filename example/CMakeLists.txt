project(example)

add_executable(test
    main.cpp
)

# Find systemd library
# Need to install PkgConfig, sudo apt-get install pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

target_link_libraries(test PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

add_dependencies(test ssdbus)