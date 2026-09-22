# CMake version >= 3.15
cmake_minimum_required(VERSION 3.15)

project(dxxcpp)

# C++ version >= 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dxxcpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Sema.cpp
    src/Codegen.cpp
    main.cpp
)

set(
    DXXCPP_INCLUDE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(dxxcpp
    PRIVATE
        ${DXXCPP_INCLUDE_DIR}
)

# Semantic layer regression validation (positive and negative examples):
# run manually or with: ctest -R dxxcpp_verify
option(DXXCPP_BUILD_TESTS "Build dxxcpp sema verify tests" ON)

if(DXXCPP_BUILD_TESTS)
    enable_testing()

    add_executable(dxxcpp_verify_golden
        src/Sema.cpp
        tests/VerifyGolden.cpp
    )
    target_include_directories(dxxcpp_verify_golden PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_golden COMMAND dxxcpp_verify_golden)

    add_executable(dxxcpp_verify_negative
        src/Sema.cpp
        tests/VerifyNegative.cpp
    )
    target_include_directories(dxxcpp_verify_negative PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_negative COMMAND dxxcpp_verify_negative)

    add_executable(dxxcpp_verify_codegen
        src/Sema.cpp
        src/Codegen.cpp
        tests/VerifyCodegen.cpp
    )
    target_include_directories(dxxcpp_verify_codegen PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_codegen COMMAND dxxcpp_verify_codegen)

    add_executable(dxxcpp_verify_lexer
        src/Lexer.cpp
        tests/VerifyLexer.cpp
    )
    target_include_directories(dxxcpp_verify_lexer PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_lexer COMMAND dxxcpp_verify_lexer)

    add_executable(dxxcpp_verify_parser
        src/Lexer.cpp
        src/Parser.cpp
        tests/VerifyParser.cpp
    )
    target_include_directories(dxxcpp_verify_parser PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_parser COMMAND dxxcpp_verify_parser)

    # ---- end to end: .dxx text -> Lexer -> Parser -> Sema -> Codegen ----
    add_executable(dxxcpp_verify_pipeline
        src/Lexer.cpp
        src/Parser.cpp
        src/Sema.cpp
        src/Codegen.cpp
        tests/VerifyPipeline.cpp
    )
    target_include_directories(dxxcpp_verify_pipeline PRIVATE ${DXXCPP_INCLUDE_DIR})
    add_test(NAME dxxcpp_verify_pipeline
        COMMAND dxxcpp_verify_pipeline ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx)

    # ---- CLI: .dxx -> headers (lexical/grammar/semantic/codegen chain) ----
    set(DXXCPP_CLI_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/cli_generated)

    # The output directory must start empty: a file left over from an earlier
    # run would hide a rename (a stale Types.hpp used to mask the
    # package-prefixed header name)
    add_test(NAME dxxcpp_cli_clean
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${DXXCPP_CLI_OUT_DIR})
    set_tests_properties(dxxcpp_cli_clean PROPERTIES FIXTURES_SETUP dxxcpp_cli_clean)

    add_test(NAME dxxcpp_cli
        COMMAND dxxcpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx
            --output-dir=${DXXCPP_CLI_OUT_DIR})
    set_tests_properties(dxxcpp_cli PROPERTIES
        FIXTURES_REQUIRED dxxcpp_cli_clean FIXTURES_SETUP dxxcpp_cli_out)

    add_test(NAME dxxcpp_cli_outputs
        COMMAND ${CMAKE_COMMAND} -DOUT_DIR=${DXXCPP_CLI_OUT_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckCliOutputs.cmake)
    set_tests_properties(dxxcpp_cli_outputs PROPERTIES
        FIXTURES_REQUIRED dxxcpp_cli_out)

    # Bad input must exit non-zero (WILL_FAIL turns that failure into a pass)
    add_test(NAME dxxcpp_cli_bad_input
        COMMAND dxxcpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/Bad.dxx -o ${DXXCPP_CLI_OUT_DIR})
    set_tests_properties(dxxcpp_cli_bad_input PROPERTIES WILL_FAIL TRUE)

    # ---- CLI exit codes: 0 ok / 1 .dxx error / 2 usage error ----
    # ctest only sees zero / non-zero, so these loop back into CMake
    function(dxxcpp_add_exit_code_test name expect)
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                -DEXE=$<TARGET_FILE:dxxcpp> -DEXPECT=${expect} "-DARGS=${ARGN}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckExitCode.cmake)
    endfunction()

    dxxcpp_add_exit_code_test(dxxcpp_cli_help 0 --help)
    dxxcpp_add_exit_code_test(dxxcpp_cli_no_input 2)
    dxxcpp_add_exit_code_test(dxxcpp_cli_unknown_option 2
        --nope ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx)
    dxxcpp_add_exit_code_test(dxxcpp_cli_two_inputs 2
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx)
    dxxcpp_add_exit_code_test(dxxcpp_cli_empty_output_dir 2
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx --output-dir=)
    dxxcpp_add_exit_code_test(dxxcpp_cli_missing_input 1
        ${CMAKE_CURRENT_BINARY_DIR}/no_such_file.dxx)

    # ---- --list-outputs: the names (and their order) drive the CMake helper ----
    add_test(NAME dxxcpp_cli_list_outputs
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:dxxcpp>
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx
            -DSIDE_EFFECT_DIR=${CMAKE_CURRENT_BINARY_DIR}/no_such_dir
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckListOutputs.cmake)

    dxxcpp_add_exit_code_test(dxxcpp_cli_list_outputs_no_input 2 --list-outputs)
    dxxcpp_add_exit_code_test(dxxcpp_cli_list_outputs_bad_input 1
        --list-outputs ${CMAKE_CURRENT_SOURCE_DIR}/tests/Bad.dxx)

    # -o<dir> attached form, and the nested output directory is created
    set(DXXCPP_CLI_ATTACHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/cli_attached/nested)

    # Same reason as dxxcpp_cli_clean: the nested dir is created by the tool
    add_test(NAME dxxcpp_cli_attached_clean
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/cli_attached)
    set_tests_properties(dxxcpp_cli_attached_clean PROPERTIES
        FIXTURES_SETUP dxxcpp_cli_attached_clean)

    add_test(NAME dxxcpp_cli_attached_output_dir
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:dxxcpp> -DEXPECT=0
            "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx;-o${DXXCPP_CLI_ATTACHED_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckExitCode.cmake)
    set_tests_properties(dxxcpp_cli_attached_output_dir PROPERTIES
        FIXTURES_REQUIRED dxxcpp_cli_attached_clean FIXTURES_SETUP dxxcpp_cli_attached)

    add_test(NAME dxxcpp_cli_attached_outputs
        COMMAND ${CMAKE_COMMAND} -DOUT_DIR=${DXXCPP_CLI_ATTACHED_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckCliOutputs.cmake)
    set_tests_properties(dxxcpp_cli_attached_outputs PROPERTIES
        FIXTURES_REQUIRED dxxcpp_cli_attached)

    # ---- truncation fuzz: every prefix of Sample.dxx must exit 0 or 1 ----
    # For a sanitizer run, call FuzzTruncate.sh with the ASan build by hand
    add_test(NAME dxxcpp_fuzz_truncate
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/FuzzTruncate.sh
            $<TARGET_FILE:dxxcpp> ${CMAKE_CURRENT_SOURCE_DIR}/tests/Sample.dxx)
    set_tests_properties(dxxcpp_fuzz_truncate PROPERTIES
        LABELS fuzz TIMEOUT 300)

    # ---- generated-code compile check: the CLI output must compile against dbusxx ----
    set(
        DXXCPP_DBUSXX_INCLUDE_DIR
            ${CMAKE_CURRENT_SOURCE_DIR}/../../library/include
    )

    if(EXISTS ${DXXCPP_DBUSXX_INCLUDE_DIR})
        # Generated code includes <dbusxx/...>, the repo headers are flat, so build a shim dir
        set(DXXCPP_SHIM_DIR ${CMAKE_CURRENT_BINARY_DIR}/dbusxx_shim)
        file(MAKE_DIRECTORY ${DXXCPP_SHIM_DIR})
        file(REMOVE ${DXXCPP_SHIM_DIR}/dbusxx)
        file(CREATE_LINK ${DXXCPP_DBUSXX_INCLUDE_DIR}
             ${DXXCPP_SHIM_DIR}/dbusxx SYMBOLIC)

        # ---- generated-code compile check: the CLI output must compile ----
        # Two views in one test: the consumer view (CompileCheckSampleMain.cpp
        # includes the generated headers) and the library view (every generated
        # source, whose out-of-line definitions a Proxy/Skeleton .so / .a is
        # built from)
        add_test(NAME dxxcpp_cli_compile_check
            COMMAND ${CMAKE_COMMAND}
                -DCXX=${CMAKE_CXX_COMPILER}
                -DSAMPLE_MAIN=${CMAKE_CURRENT_SOURCE_DIR}/tests/CompileCheckSampleMain.cpp
                -DOUT_DIR=${DXXCPP_CLI_OUT_DIR}
                -DSHIM_DIR=${DXXCPP_SHIM_DIR}
                -DDBUSXX_INCLUDE_DIR=${DXXCPP_DBUSXX_INCLUDE_DIR}
                -DSERVICE=com.example.app
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckCompile.cmake)
        set_tests_properties(dxxcpp_cli_compile_check PROPERTIES
            FIXTURES_REQUIRED dxxcpp_cli_out)
    endif()
endif()

# ---- install the generator ----
# Consumers look for it next to the library (<prefix>/bin) via find_program;
# the headers it emits are tied to the matching library version
include(GNUInstallDirs)

install(TARGETS dxxcpp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})