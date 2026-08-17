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
        dbusxx_opts
        dbusxx
)

target_link_libraries(example_server
    PRIVATE
        dbusxx_opts
        dbusxx
)

target_link_libraries(example_register
    PRIVATE
        dbusxx_opts
        dbusxx
)

target_link_libraries(example_client_internal
    PRIVATE
        dbusxx_opts
        dbusxx
)

target_link_libraries(example_client_external
    PRIVATE
        dbusxx_opts
        dbusxx
)

target_link_libraries(example_peer
    PRIVATE
        dbusxx_opts
        dbusxx
)

add_dependencies(example_session dbusxx)
add_dependencies(example_server dbusxx)
add_dependencies(example_register dbusxx)
add_dependencies(example_client_internal dbusxx)
add_dependencies(example_client_external dbusxx)
add_dependencies(example_peer dbusxx)