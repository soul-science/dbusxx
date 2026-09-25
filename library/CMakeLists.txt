set(
    INCLUDES_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(
    SOURCES
        src/private/adaptor/RawBusSharePtr.cpp
        src/private/adaptor/RawEventSharePtr.cpp
        src/private/adaptor/RawMessageSharePtr.cpp
        src/private/adaptor/RawSlotSharePtr.cpp
        src/private/adaptor/RawRemoteError.cpp
        src/private/adaptor/VTableRegistrar.cpp
        src/private/message/MessagePrivate.cpp
        src/private/message/PropertyHandler.cpp
        src/private/message/ReplyAsyncHandler.cpp
        src/private/method/Reconnect.cpp
        src/private/session/LooperPrivate.cpp
        src/private/session/SessionPrivate.cpp
        src/Client.cpp
        src/Message.cpp
        src/Looper.cpp
        src/Session.cpp
)

add_library(dbusxx
    SHARED
        ${SOURCES}
)

set_target_properties(dbusxx PROPERTIES
    OUTPUT_NAME dbusxx
    VERSION 1.0.0
    SOVERSION 1
    POSITION_INDEPENDENT_CODE ON
    # CXX_VISIBILITY_PRESET hidden
    # VISIBILITY_INLINES_HIDDEN ON
)

# Find systemd library
# Need to install PkgConfig, sudo apt-get install pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd>=249)
if(NOT SYSTEMD_FOUND)
    message(
        FATAL_ERROR
            "libsystemd >= 249 is required (found ${SYSTEMD_VERSION})"
    )
endif()

include(GNUInstallDirs)

# Include header file path
target_include_directories(dbusxx
    PUBLIC
        # Use header files from the source code directory during construction
        $<BUILD_INTERFACE:${INCLUDES_DIR}>
        # After installation, headers live in <includedir>/dbusxx/. Export two
        # roots so both include styles work for consumers:
        #   - `#include <dbusxx/Reply.hpp>`              -> needs the parent dir
        #   - internal `"Status.hpp"` / `"private/..."`  -> needs the dbusxx dir
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dbusxx>
        # systemd headers
        ${SYSTEMD_INCLUDE_DIRS}
)

# Link to libraries
target_link_libraries(dbusxx
    PUBLIC
        ${SYSTEMD_LIBRARIES}
)

target_link_libraries(dbusxx
    PRIVATE
        dbusxx_opts
)

target_compile_options(dbusxx
    PRIVATE
        ${SYSTEMD_CFLAGS_OTHER}
)

# Install the shared library into <prefix>/${CMAKE_INSTALL_LIBDIR}
install(
    TARGETS dbusxx
    # Export dbusxxTargets and store target usage information
    EXPORT dbusxxTargets
    # Installation location: <prefix>/${CMAKE_INSTALL_LIBDIR}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install header files to <prefix>/${CMAKE_INSTALL_INCLUDEDIR}/dbusxx
install(
    DIRECTORY
        ${INCLUDES_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dbusxx
    FILES_MATCHING PATTERN
        "*.hpp"
)

set(DBUSXX_CMAKE_PACKAGE_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/dbusxx")
# Install exported CMake Targets, enable 'find_package' to locate
install(
    # Install dbusxxTargets.cmake
    EXPORT dbusxxTargets
    # Determine the export file for Target
    FILE dbusxxTargets.cmake
    # Installation location: <prefix>/${DBUSXX_CMAKE_PACKAGE_DIR}
    DESTINATION ${DBUSXX_CMAKE_PACKAGE_DIR}
)

# Introducing auxiliary functions -> configure_package_config_file
include(CMakePackageConfigHelpers)

# Generate dbusxxConfig.cmake, 'find_package' will use it
configure_package_config_file(
    # Template file
    "${CMAKE_CURRENT_SOURCE_DIR}/dbusxxConfig.cmake.in"
    # Output file
    "${CMAKE_CURRENT_BINARY_DIR}/dbusxxConfig.cmake"
    # Installation path
    INSTALL_DESTINATION
        ${DBUSXX_CMAKE_PACKAGE_DIR}
)

# Install dbusxxConfig.cmake
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/dbusxxConfig.cmake"
    DESTINATION
        ${DBUSXX_CMAKE_PACKAGE_DIR}
)

# Install the generator helper: 'find_package(dbusxx)' -> dxxcpp_generate_lib()
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dxxcppGenerator.cmake"
    DESTINATION
        ${DBUSXX_CMAKE_PACKAGE_DIR}
)