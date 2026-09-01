# Benchmarks — Google Benchmark based. Spans three layers:
#   * pure-logic layer   (Status / statusMessage)              — no D-Bus
#   * serialization layer (MessagePrivate write/read/round-trip) — needs a bus
#   * business E2E layer  (Server/Client sync/async/property/signal) — needs
#     a bus daemon, which BenchmarkMain.cpp spawns *privately*, so no system
#     D-Bus daemon is required.
#
# Google Benchmark resolution order:
#   1. System install via `find_package(benchmark)` (e.g. apt libbenchmark-dev)
#   2. pkg-config `benchmark`
#   3. FetchContent from GitHub (fallback only when neither is available)
# Everything links the in-tree `dbusxx` shared library (which propagates its
# PUBLIC systemd include dirs & libs) plus `dbusxx_opts` for consistent
# compiler options (Debug sanitizers, Release LTO).

# ---- 1. system-installed Google Benchmark ------------------------------------
find_package(benchmark QUIET)
if(TARGET benchmark::benchmark)
    set(DBUSXX_BENCH_LINK benchmark::benchmark)
    message(STATUS "dbusxx benchmarks: using system Google Benchmark (find_package)")
else()
    # ---- 2. pkg-config fallback ----------------------------------------------
    find_package(PkgConfig QUIET)
    pkg_check_modules(BENCHMARK QUIET IMPORTED_TARGET benchmark)
    if(TARGET PkgConfig::BENCHMARK)
        set(DBUSXX_BENCH_LINK PkgConfig::BENCHMARK)
        message(STATUS "dbusxx benchmarks: using system Google Benchmark (pkg-config)")
    else()
        # ---- 3. fallback: fetch from GitHub at configure time ---------------
        message(STATUS "dbusxx benchmarks: no system Google Benchmark, will FetchContent from GitHub")
        include(FetchContent)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG        v1.8.3
        )
        FetchContent_MakeAvailable(googlebenchmark)
        set(DBUSXX_BENCH_LINK benchmark::benchmark)
    endif()
endif()

add_executable(sdbusxx_benchmarks
    BenchmarkMain.cpp
    BenchmarkStatus.cpp
    BenchmarkMessage.cpp
    BenchmarkLooper.cpp
    BenchmarkE2E.cpp
)

target_include_directories(sdbusxx_benchmarks
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sdbusxx_benchmarks
    PRIVATE
        dbusxx
        dbusxx_opts
        # BenchmarkMain.cpp provides the entry point, so link Google Benchmark
        # *without* its own main().
        ${DBUSXX_BENCH_LINK}
)

# Registered with CTest but DISABLED by default: benchmarks are long-running,
# so a plain `ctest` should only run the fast unit tests. Run them explicitly:
#   cmake --build build_release --target sdbusxx_benchmarks
#   ./build_release/tests/benchmark_tests/sdbusxx_benchmarks \
#       --benchmark_filter='E2EBench/*' --benchmark_min_time=0.1
add_test(NAME sdbusxx_benchmarks COMMAND sdbusxx_benchmarks --benchmark_min_time=0.2)
set_tests_properties(sdbusxx_benchmarks PROPERTIES DISABLED TRUE)
