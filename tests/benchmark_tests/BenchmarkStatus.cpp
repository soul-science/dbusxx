//! Benchmarks for the pure-logic layer: Status construction, code inspection
//! and the `statusMessage()` lookup. No D-Bus daemon required.
//!
//! These are micro-benchmarks of cheap in-memory operations, so the reported
//! numbers are mainly useful for detecting regressions / comparing code paths,
//! not as absolute throughput figures.

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <string>

#include "Status.hpp"

using namespace Dbusxx;

namespace {

//! Every status code, pre-materialised so benchmarks are not dominated by
//! index arithmetic. Sized to a power of two (32) so the `& (size()-1)`
//! wrap-around in the loops covers every distinct code: the first 20 entries
//! are the unique codes, the tail repeats them to fill the array.
constexpr std::array<StatusCode, 32> kAllCodes = {
    StatusCode::SUCCESS,
    StatusCode::INVALID_ARG,
    StatusCode::NOT_FOUND,
    StatusCode::NO_SERVICE,
    StatusCode::NO_METHOD,
    StatusCode::ACCESS_DENIED,
    StatusCode::NAME_EXISTS,
    StatusCode::NOT_CONNECTED,
    StatusCode::CONN_RESET,
    StatusCode::BUSY,
    StatusCode::TIMEOUT,
    StatusCode::NO_MEMORY,
    StatusCode::NO_REPLY,
    StatusCode::IO_ERROR,
    StatusCode::MSG_TOO_LONG,
    StatusCode::LIMIT_EXCEEDED,
    StatusCode::PROTOCOL_ERROR,
    StatusCode::TYPE_MISMATCH,
    StatusCode::DISCONNECTED,
    StatusCode::UNKNOWN_ERROR,
    //! --- repeats to reach 32 (power of two mask) ---
    StatusCode::SUCCESS,
    StatusCode::INVALID_ARG,
    StatusCode::NOT_FOUND,
    StatusCode::NO_SERVICE,
    StatusCode::NO_METHOD,
    StatusCode::ACCESS_DENIED,
    StatusCode::NAME_EXISTS,
    StatusCode::NOT_CONNECTED,
    StatusCode::CONN_RESET,
    StatusCode::BUSY,
    StatusCode::TIMEOUT,
    StatusCode::NO_MEMORY,
};

} //! namespace

//! Construct a Status from a raw code (the hottest path in error propagation).
static void statusConstruct(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        Status st(kAllCodes[i++ & (kAllCodes.size() - 1)]);
        benchmark::DoNotOptimize(st);
    }
}
BENCHMARK(statusConstruct);

//! Construct + inspect isSuccess()/isError()/code() — what callers actually
//! do after every operation.
static void statusInspect(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        Status st(kAllCodes[i++ & (kAllCodes.size() - 1)]);
        benchmark::DoNotOptimize(st.isSuccess());
        benchmark::DoNotOptimize(st.isError());
        benchmark::DoNotOptimize(st.code());
    }
}
BENCHMARK(statusInspect);

//! Direct constexpr switch lookup for the human-readable message.
static void statusMessageLookup(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        const char* m = statusMessage(kAllCodes[i++ & (kAllCodes.size() - 1)]);
        benchmark::DoNotOptimize(m);
    }
}
BENCHMARK(statusMessageLookup);

//! Status::message() — the wrapper that also materialises a std::string.
static void statusMessageString(benchmark::State& state) {
    size_t i = 0;
    for (auto _ : state) {
        Status st(kAllCodes[i++ & (kAllCodes.size() - 1)]);
        std::string msg = st.message();
        benchmark::DoNotOptimize(msg);
    }
}
BENCHMARK(statusMessageString);

//! Copying a Status around (value semantics, used when storing results).
static void statusCopy(benchmark::State& state) {
    Status src(StatusCode::TIMEOUT);
    for (auto _ : state) {
        Status dst = src;
        benchmark::DoNotOptimize(dst);
    }
}
BENCHMARK(statusCopy);
