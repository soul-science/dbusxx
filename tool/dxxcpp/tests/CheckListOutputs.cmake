# Verify 'dxxcpp --list-outputs': the exact names in generation order, exit 0,
# and no side effect on the file system (the CMake helper relies on all three).
# usage: cmake -DEXE=<dxxcpp> -DINPUT=<file.dxx> -DSIDE_EFFECT_DIR=<absent dir> -P <this>

foreach(_required IN ITEMS EXE INPUT SIDE_EFFECT_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${EXE}" --list-outputs "${INPUT}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "--list-outputs exited ${_rc}: ${_err}")
endif()

string(REPLACE "\n" ";" _names "${_out}")
set(_expected
    ComExampleCalcTypes.hpp
    CalculatorSkeleton.hpp
    CalculatorSkeleton.cpp
    CalculatorProxy.hpp
    CalculatorProxy.cpp
    LoggerSkeleton.hpp
    LoggerSkeleton.cpp
    LoggerProxy.hpp
    LoggerProxy.cpp
)
if(NOT _names STREQUAL _expected)
    message(FATAL_ERROR "names mismatch\n  got:      ${_names}\n  expected: ${_expected}")
endif()

#! It must not create the output directory either, even when -o is given
file(REMOVE_RECURSE "${SIDE_EFFECT_DIR}")
execute_process(
    COMMAND "${EXE}" --list-outputs "${INPUT}" -o "${SIDE_EFFECT_DIR}"
    RESULT_VARIABLE _rc2
    OUTPUT_QUIET
    ERROR_VARIABLE _err2
)
if(NOT _rc2 EQUAL 0)
    message(FATAL_ERROR "--list-outputs -o exited ${_rc2}: ${_err2}")
endif()

if(EXISTS "${SIDE_EFFECT_DIR}")
    message(FATAL_ERROR "--list-outputs must not create '${SIDE_EFFECT_DIR}'")
endif()

message(STATUS "list-outputs ok: ${_names}")
