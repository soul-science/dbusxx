# Check the exit code of a command (used by the dxxcpp CLI tests).
#
# ctest itself cannot tell 0 / 1 / 2 apart (WILL_FAIL only distinguishes zero
# from non-zero), so these tests loop back into CMake and compare the code.
#
# usage: cmake -DEXE=<binary> -DEXPECT=<code> [-DARGS=a;b;c] -P CheckExitCode.cmake

if(NOT DEFINED EXE OR NOT DEFINED EXPECT)
    message(FATAL_ERROR "EXE and EXPECT are required")
endif()

execute_process(
    COMMAND ${EXE} ${ARGS}
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE CODE)

if(NOT "${CODE}" STREQUAL "${EXPECT}")
    message(FATAL_ERROR "expected exit code ${EXPECT}, got '${CODE}'")
endif()

message(STATUS "exit code ${CODE} as expected")
