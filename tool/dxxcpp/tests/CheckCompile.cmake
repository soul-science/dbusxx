# Compile the CLI output (syntax only), from two angles:
#   1. consumer view -- a hand-written TU that includes the generated headers
#   2. library view  -- every generated source, whose out-of-line definitions are
#                       what a Proxy/Skeleton .so / .a is built from
# usage: cmake -DCXX=<compiler> -DSAMPLE_MAIN=<file.cpp> -DOUT_DIR=<dir>
#              -DSHIM_DIR=<dir> -DDBUSXX_INCLUDE_DIR=<dir> -DSERVICE=<name> -P <this>

foreach(_required IN ITEMS CXX SAMPLE_MAIN OUT_DIR SHIM_DIR DBUSXX_INCLUDE_DIR SERVICE)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

file(GLOB _sources "${OUT_DIR}/*.cpp")
if(NOT _sources)
    message(FATAL_ERROR "no generated source in ${OUT_DIR}")
endif()

#! One invocation for every TU: the compiler reports the file of a failure
#! itself, so a per-file loop would only add noise
execute_process(
    COMMAND "${CXX}" -std=c++17 -fsyntax-only -Werror=deprecated-declarations
        "-DDBUSXX_SERVICE_NAME=\"${SERVICE}\""
        -I "${OUT_DIR}" -I "${SHIM_DIR}" -I "${DBUSXX_INCLUDE_DIR}"
        "${SAMPLE_MAIN}" ${_sources}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the generated code does not compile\n${_out}\n${_err}")
endif()

list(LENGTH _sources _count)
message(STATUS "compiled ${_count} generated source(s) + ${SAMPLE_MAIN}")
