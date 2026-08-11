project(example)


add_executable(example_session
    example_session.cpp
)

add_executable(example_server
    example_server.cpp
)

add_executable(example_register
    example_register.cpp
)

add_executable(example_client_internal
    example_client_internal.cpp
)

add_executable(example_client_external
    example_client_external.cpp
)

add_executable(example_peer
    example_peer.cpp
)

# Find systemd library
# Need to install PkgConfig, sudo apt-get install pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

target_link_libraries(example_session PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

target_link_libraries(example_server PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

target_link_libraries(example_register PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

target_link_libraries(example_client_internal PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

target_link_libraries(example_client_external PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

target_link_libraries(example_peer PRIVATE
    ${SYSTEMD_LIBRARIES}
    ssdbus
)

add_dependencies(example_session ssdbus)
add_dependencies(example_server ssdbus)
add_dependencies(example_register ssdbus)
add_dependencies(example_client_internal ssdbus)
add_dependencies(example_client_external ssdbus)
add_dependencies(example_peer ssdbus)