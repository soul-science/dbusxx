project(lib)

set(SOURCES
    src/DbusContext.cpp
    src/DbusUtils.cpp
)

add_library(ssdbus SHARED ${SOURCES})

# Find systemd library
# Need to install PkgConfig, sudo apt-get install pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

set(INCLUDES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Include header file path
target_include_directories(ssdbus PUBLIC
    $<BUILD_INTERFACE:${INCLUDES_DIR}>  # Use header files from the source code directory during construction
    $<INSTALL_INTERFACE:include/ssdbus>  # After installation, use the installation directory /usr/local/include/
)

# Link to libraries
target_link_libraries(ssdbus PRIVATE
    ${SYSTEMD_LIBRARIES}
)

# Compile configuration optimization
target_compile_options(ssdbus PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-O3 -DNDEBUG -flto -march=native>
    ${SYSTEMD_CFLAGS_OTHER}
)

# Install .so to/usr/local/lib
install(TARGETS ssdbus
    EXPORT ssdbusTargets          # Export ssdbusTargets and store target usage information
    DESTINATION lib              # Installation location: /usr/local/lib
)

# Install header files to /usr/local/include/ssdbus
install(FILES
    ${INCLUDES_DIR}/DbusContext.hpp
    DESTINATION include/ssdbus
)

# Install exported CMake Targets, enable 'find_package' to locate
    install(EXPORT ssdbusTargets      # Install ssdbusTargets.cmake
    FILE ssdbusTargets.cmake      # Determine the export file for Target
    DESTINATION lib/cmake/ssdbus  # Installation location: /usr/local/lib/cmake/ssdbus
)

# Introducing auxiliary functions -> configure_package_config_file
include(CMakePackageConfigHelpers)

# Generate ssdbusConfig.cmake, 'find_package' will use it
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ssdbusConfig.cmake.in"   # Template file
    "${CMAKE_CURRENT_BINARY_DIR}/ssdbusConfig.cmake"      # Output file
    INSTALL_DESTINATION lib/cmake/ssdbus                  # Installation path
)

# Install ssdbusConfig.cmake
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ssdbusConfig.cmake"
    DESTINATION lib/cmake/ssdbus
)