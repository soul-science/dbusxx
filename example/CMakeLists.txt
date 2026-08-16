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

target_link_libraries(example_session
    PRIVATE
        ssdbus_opts
        ssdbus
)

target_link_libraries(example_server
    PRIVATE
        ssdbus_opts
        ssdbus
)

target_link_libraries(example_register
    PRIVATE
        ssdbus_opts
        ssdbus
)

target_link_libraries(example_client_internal
    PRIVATE
        ssdbus_opts
        ssdbus
)

target_link_libraries(example_client_external
    PRIVATE
        ssdbus_opts
        ssdbus
)

target_link_libraries(example_peer
    PRIVATE
        ssdbus_opts
        ssdbus
)

add_dependencies(example_session ssdbus)
add_dependencies(example_server ssdbus)
add_dependencies(example_register ssdbus)
add_dependencies(example_client_internal ssdbus)
add_dependencies(example_client_external ssdbus)
add_dependencies(example_peer ssdbus)