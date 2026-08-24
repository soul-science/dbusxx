# Unit tests — Google Test based. Covers the pure-logic layers (Status, type
# extraction, FuncTrait, error mapping, message serialization) plus
# business-layer end-to-end (Looper/Session/Server/Client). main.cpp spawns a
# *private* dbus-daemon, so no system D-Bus daemon is required.
#
# Google Test resolution order:
#   1. System install via `find_package(GTest)` (e.g. apt libgtest-dev,
#      compiled & installed to /usr/local)
#   2. Local source tree at <repo>/tests/unit_tests/third_party/googletest
#      (clone it yourself once, configure then needs no network)
#   3. FetchContent from GitHub (fallback only when neither is available)
# Everything links the in-tree `dbusxx` shared library, which propagates its
# PUBLIC systemd include dirs & libs, plus `dbusxx_opts` for consistent
# compiler options (Debug sanitizers, Release LTO).

# ---- 1. system-installed Google Test ----------------------------------------
find_package(GTest QUIET)
if(TARGET GTest::gtest_main)
    message(STATUS "dbusxx unit tests: using system Google Test (find_package)")
else()
    # ---- 2. locally downloaded googletest source ----------------------------
    set(DBUSXX_GTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest")
    if(EXISTS "${DBUSXX_GTEST_DIR}/CMakeLists.txt")
        message(STATUS "dbusxx unit tests: using local googletest at ${DBUSXX_GTEST_DIR}")
        add_subdirectory(
            ${DBUSXX_GTEST_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}/googletest
        )
    else()
        # ---- 3. fallback: fetch from GitHub at configure time ---------------
        message(STATUS "dbusxx unit tests: no system/local googletest, will FetchContent from GitHub")
        include(FetchContent)

        # Keep our tree's gtest pristine and dependency-light.
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )

        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# Keep the gtest static libs consistent with the rest of the Debug build
# (ASan/UBSan) so sanitizer coverage extends into gtest internals too.
# Only applies to in-tree-built gtest (add_subdirectory / FetchContent);
# a system-installed gtest is left untouched.
foreach(gtest_tgt gtest gtest_main gmock gmock_main)
    if(TARGET ${gtest_tgt} AND NOT TARGET GTest::gtest_main)
        target_link_libraries(${gtest_tgt} PUBLIC dbusxx_opts)
        target_compile_options(${gtest_tgt} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    endif()
endforeach()

add_executable(sdbusxx_unit_tests
    main.cpp
    TestStatus.cpp
    TestArgs.cpp
    TestFuncTrait.cpp
    TestRawError.cpp
    TestLooper.cpp
    TestSession.cpp
    TestMessage.cpp
    TestPropertyWrapper.cpp
    TestServerClient.cpp
)

target_include_directories(sdbusxx_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sdbusxx_unit_tests
    PRIVATE
        dbusxx
        dbusxx_opts
        # Custom main.cpp provides the entry point, so link gtest *without*
        # gtest_main (which would define a conflicting main()).
        GTest::gtest
        # NB: gmock is intentionally NOT linked — the apt libgtest-dev on
        # this machine ships gtest only. No test here uses gmock mocks.
)

add_test(NAME sdbusxx_unit_tests COMMAND sdbusxx_unit_tests)
