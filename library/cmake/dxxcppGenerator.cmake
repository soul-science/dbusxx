# Install prefix, handed over by dbusxxConfig.cmake (it knows it from
# PACKAGE_PREFIX_DIR).  The '/../../..' fallback is only for including this file
# straight out of a build tree, and it assumes <libdir> is one path component
# (lib or lib64) - not lib/<multiarch>, hence the hand-over.
if(NOT DEFINED DBUSXX_PREFIX)
    get_filename_component(DBUSXX_PREFIX
        "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()

find_program(DXXCPP_EXE
    NAMES dxxcpp
    HINTS "${DBUSXX_PREFIX}/bin"
    DOC "dxxcpp: .dxx -> dbusxx C++ header generator")

# ---- dxxcpp_generate_lib(): one .dxx -> the libs a service/SDK needs ----
# Generates the C++ sources of <INPUT> and creates the three targets a service
# and its clients live on:
#   <LIB_PREFIX>_dxx_types   INTERFACE  <Package>Types.hpp, both install
#                                       include roots, DBUSXX_SERVICE_NAME,
#                                       dbusxx
#   <LIB_PREFIX>_dxx_server  STATIC     <Iface>Skeleton.hpp/.cpp  (internal)
#   <LIB_PREFIX>_dxx_client  SHARED     <Iface>Proxy.hpp/.cpp     [installed]
# Both libraries are always built; INSTALL_CLIENT only decides what the install
# tree receives.  With it on (the default), another project finds the package
# again with 'find_package(<LIB_PREFIX>_dxx CONFIG)' and links
# '<LIB_PREFIX>_dxx_client' - it needs no DBUSXX_SERVICE_NAME of its own, the
# types target already carries it.  File names come from the .dxx and not from
# LIB_PREFIX: '<Package>Types.hpp' uses its declared package and
# '<Iface>Skeleton/Proxy.hpp' the interface names; consumers include them by
# bare name, the types target puts <include>/<LIB_PREFIX>_dxx on the search
# path.  Installed: those headers under <include>/<LIB_PREFIX>_dxx,
# <lib>/lib<LIB_PREFIX>_dxx_client.so and
# <lib>/cmake/<LIB_PREFIX>_dxx/{Config,Targets,Targets-noconfig}.cmake; the
# generated Config.cmake re-finds 'dbusxx' with find_dependency, so that install
# has to be discoverable as well.  The server side is never published: whoever
# implements the same interface generates the skeleton from the .dxx.
#
#   LIB_PREFIX <prefix>     required, unique per call; names the targets, the
#                           package and the installed include directory
#   INPUT <file.dxx>        required, the interface description
#   SERVICE <name>          required, the D-Bus well-known name to build in
#   OUTPUT_DIR <dir>        where the sources are generated; defaults to
#                           '${CMAKE_CURRENT_BINARY_DIR}/dxx_<stem>'.  Every
#                           .dxx needs its own directory, and one
#                           (INPUT, OUTPUT_DIR) pair may only be claimed once:
#                           reusing it from the same directory builds further
#                           libs (e.g. another SERVICE) on the same sources,
#                           reusing it from elsewhere is an error because the
#                           generation rule is scoped to the declaring dir
#   INSTALL_CLIENT <on|off> default on; off keeps everything build-only
#
# Needs 'find_package(dbusxx)' first (the generated code links 'dbusxx') and
# dxxcpp installed next to it (or pass -DDXXCPP_EXE=<path>).
#
#   dxxcpp_generate_lib(LIB_PREFIX hello INPUT hello.dxx
#       SERVICE com.example.hello)
#   target_link_libraries(hello_service PRIVATE hello_dxx_server)

function(dxxcpp_generate_lib)
    cmake_parse_arguments(DXX ""
        "LIB_PREFIX;INPUT;SERVICE;OUTPUT_DIR;INSTALL_CLIENT" "" ${ARGN})

    if(DXX_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: unknown argument(s): "
            "${DXX_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT DXX_LIB_PREFIX)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: LIB_PREFIX <lib_prefix> is required")
    endif()

    set(DXX_GENERATED_TARGET_PREFIX "${DXX_LIB_PREFIX}_dxx")
    if(TARGET ${DXX_GENERATED_TARGET_PREFIX}_types)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: LIB_PREFIX '${DXX_LIB_PREFIX}' is already "
            "used by another dxxcpp_generate_lib() call")
    endif()

    if(NOT DXX_SERVICE)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: SERVICE <service_name> is required")
    endif()

    if(NOT DXX_INPUT)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: INPUT <file.dxx> is required")
    endif()

    # Must use absolute path
    get_filename_component(DXX_INPUT "${DXX_INPUT}" ABSOLUTE)
    if(NOT EXISTS "${DXX_INPUT}")
        message(FATAL_ERROR
            "dxxcpp_generate_lib: INPUT '${DXX_INPUT}' does not exist")
    endif()

    if(NOT TARGET dbusxx)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: target 'dbusxx' is missing; "
            "call find_package(dbusxx) before this function")
    endif()

    if(NOT DEFINED DXX_INSTALL_CLIENT)
        set(DXX_INSTALL_CLIENT ON)
    endif()

    if(DXXCPP_EXE MATCHES "-NOTFOUND$")
        message(FATAL_ERROR
            "dxxcpp_generate_lib: dxxcpp not found; "
            "pass -DDXXCPP_EXE=<path> or install it next to dbusxx")
    endif()

    if(NOT DXX_OUTPUT_DIR)
        # Get the name of <name>.dxx
        get_filename_component(DXX_OUTPUT_STEM "${DXX_INPUT}" NAME_WE)
        set(DXX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/dxx_${DXX_OUTPUT_STEM}")
    endif()

    # Unified into absolute paths to prevent users from
    # entering relative paths and not being able to find folders
    get_filename_component(DXX_OUTPUT_DIR "${DXX_OUTPUT_DIR}" ABSOLUTE)

    # Generate cpp files via dxxcpp
    execute_process(
        COMMAND "${DXXCPP_EXE}" --list-outputs "${DXX_INPUT}"
        RESULT_VARIABLE DXXCPP_LIST_RES
        OUTPUT_VARIABLE DXXCPP_LIST_OUT
        ERROR_VARIABLE DXXCPP_LIST_ERR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT DXXCPP_LIST_RES EQUAL 0)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: dxxcpp failed on '${DXX_INPUT}':\n"
            "${DXXCPP_LIST_ERR}")
    endif()

    # Initialize generated files
    set(DXX_GENERATED_ALL_FILES "")
    set(DXX_GENERATED_TYPE_HEADERS "")
    set(DXX_GENERATED_CLIENT_HEADERS "")
    set(DXX_GENERATED_CLIENT_SOURCES "")
    set(DXX_GENERATED_SERVER_HEADERS "")
    set(DXX_GENERATED_SERVER_SOURCES "")
    string(REPLACE "\n" ";" DXX_LIST "${DXXCPP_LIST_OUT}")
    foreach(_ENTRY IN LISTS DXX_LIST)
        if(_ENTRY STREQUAL "")
            continue()
        endif()

        # Get role(types|server|client):name
        string(REPLACE ":" ";" _PAIR "${_ENTRY}")
        list(GET _PAIR 0 _ROLE)
        list(GET _PAIR 1 _NAME)

        set(_PATH "${DXX_OUTPUT_DIR}/${_NAME}")
        list(APPEND DXX_GENERATED_ALL_FILES "${_PATH}")
        if(_ROLE STREQUAL "types")
            list(APPEND DXX_GENERATED_TYPE_HEADERS "${_PATH}")
        # escape_identity: '\' <match '[^A-Za-z0-9;]'>
        # so \\. indicate '.' character
        elseif(_ROLE STREQUAL "server" AND _NAME MATCHES "\\.hpp$")
            list(APPEND DXX_GENERATED_SERVER_HEADERS "${_PATH}")
        elseif(_ROLE STREQUAL "server" AND _NAME MATCHES "\\.cpp$")
            list(APPEND DXX_GENERATED_SERVER_SOURCES "${_PATH}")
        elseif(_ROLE STREQUAL "client" AND _NAME MATCHES "\\.hpp$")
            list(APPEND DXX_GENERATED_CLIENT_HEADERS "${_PATH}")
        elseif(_ROLE STREQUAL "client" AND _NAME MATCHES "\\.cpp$")
            list(APPEND DXX_GENERATED_CLIENT_SOURCES "${_PATH}")
        else()
            message(FATAL_ERROR
                "dxxcpp_generate_lib: unexpected entry '${_ENTRY}'; "
                "expected types:/server:/client: followed by a file name")
        endif()
    endforeach()

    # Run dxxcpp to generate cpp files
    get_property(_OWNER_IN_DIR GLOBAL PROPERTY
        "DXXCPP_OWNER_${DXX_OUTPUT_DIR}")
    get_property(_DECLARED_IN_BINARY_DIR GLOBAL PROPERTY
        "DXXCPP_DECLARED_IN_${DXX_OUTPUT_DIR}")
    if(NOT DEFINED _OWNER_IN_DIR OR _OWNER_IN_DIR STREQUAL "")
        add_custom_command(
            OUTPUT ${DXX_GENERATED_ALL_FILES}
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DXX_OUTPUT_DIR}"
            COMMAND "${DXXCPP_EXE}" "${DXX_INPUT}" -o "${DXX_OUTPUT_DIR}"
            DEPENDS "${DXX_INPUT}" "${DXXCPP_EXE}"
            VERBATIM
        )
        set_property(GLOBAL PROPERTY
            "DXXCPP_OWNER_${DXX_OUTPUT_DIR}" "${DXX_INPUT}")
        set_property(GLOBAL PROPERTY
            "DXXCPP_DECLARED_IN_${DXX_OUTPUT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        # If .dxx has changes, cmake will automatically
        # reconfigure during the next build without manual configuration.
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${DXX_INPUT}")
    elseif(NOT _OWNER_IN_DIR STREQUAL DXX_INPUT)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: OUTPUT_DIR '${DXX_OUTPUT_DIR}' "
            "is already used by '${_OWNER_IN_DIR}'; "
            "every .dxx needs its own directory")
    elseif(NOT _DECLARED_IN_BINARY_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
        message(FATAL_ERROR
            "dxxcpp_generate_lib: INPUT '${DXX_INPUT}' "
            "was generated in '${_DECLARED_IN_BINARY_DIR}'; "
            "reuse it from that directory, or give this call its own "
            "OUTPUT_DIR")
    endif()

    # Create libraries(types / server / client)
    include(GNUInstallDirs)
    set(DXX_GENERATED_INCLUDEDIR
        "${CMAKE_INSTALL_INCLUDEDIR}/${DXX_GENERATED_TARGET_PREFIX}")
    set(DXX_GENERATED_TARGET_TYPES
        "${DXX_GENERATED_TARGET_PREFIX}_types")
    set(DXX_GENERATED_TARGET_SERVER
        "${DXX_GENERATED_TARGET_PREFIX}_server")
    set(DXX_GENERATED_TARGET_CLIENT
        "${DXX_GENERATED_TARGET_PREFIX}_client")

    # Create types library
    add_library(${DXX_GENERATED_TARGET_TYPES} INTERFACE)
    target_include_directories(${DXX_GENERATED_TARGET_TYPES} INTERFACE
        $<BUILD_INTERFACE:${DXX_OUTPUT_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${DXX_GENERATED_INCLUDEDIR}>
    )
    target_compile_definitions(${DXX_GENERATED_TARGET_TYPES} INTERFACE
        DBUSXX_SERVICE_NAME="${DXX_SERVICE}")
    target_link_libraries(${DXX_GENERATED_TARGET_TYPES} INTERFACE dbusxx)

    # Create server library
    add_library(${DXX_GENERATED_TARGET_SERVER} STATIC
        ${DXX_GENERATED_SERVER_HEADERS}
        ${DXX_GENERATED_SERVER_SOURCES}
    )
    target_link_libraries(${DXX_GENERATED_TARGET_SERVER} PUBLIC
        ${DXX_GENERATED_TARGET_TYPES})

    # Create client library
    add_library(${DXX_GENERATED_TARGET_CLIENT} SHARED
        ${DXX_GENERATED_CLIENT_HEADERS}
        ${DXX_GENERATED_CLIENT_SOURCES}
    )
    target_link_libraries(${DXX_GENERATED_TARGET_CLIENT} PUBLIC
        ${DXX_GENERATED_TARGET_TYPES})

    if(NOT DXX_INSTALL_CLIENT)
        return()
    endif()

    set(DXX_EXPORT_GENERATED_TARGETS
        ${DXX_GENERATED_TARGET_TYPES}
        ${DXX_GENERATED_TARGET_CLIENT}
    )
    set(DXX_EXPORT_GENERATED_HEADERS
        ${DXX_GENERATED_TYPE_HEADERS}
        ${DXX_GENERATED_CLIENT_HEADERS}
    )

    # Install headers
    install(
        FILES ${DXX_EXPORT_GENERATED_HEADERS}
        DESTINATION ${DXX_GENERATED_INCLUDEDIR}
    )

    # Install export
    set(DXX_EXPORT
        "${DXX_GENERATED_TARGET_PREFIX}Targets")
    set(DXX_EXPORT_CMAKE_DIR
        "${CMAKE_INSTALL_LIBDIR}/cmake/${DXX_GENERATED_TARGET_PREFIX}")
    install(
        TARGETS ${DXX_EXPORT_GENERATED_TARGETS}
        EXPORT ${DXX_EXPORT}
    )
    install(
        EXPORT ${DXX_EXPORT}
        FILE ${DXX_EXPORT}.cmake
        DESTINATION ${DXX_EXPORT_CMAKE_DIR}
    )

    # Install cmake config
    set(DXX_EXPORT_CONFIG_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/${DXX_GENERATED_TARGET_PREFIX}")
    set(DXX_EXPORT_CONFIG
        "${DXX_EXPORT_CONFIG_DIR}/${DXX_GENERATED_TARGET_PREFIX}Config.cmake")
    file(
        WRITE "${DXX_EXPORT_CONFIG}"
        "# Generated by dxxcpp_generate_lib (${DXX_INPUT})\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(dbusxx CONFIG)\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/${DXX_EXPORT}.cmake\")\n"
    )
    install(
        FILES "${DXX_EXPORT_CONFIG}"
        DESTINATION ${DXX_EXPORT_CMAKE_DIR}
    )
endfunction()
