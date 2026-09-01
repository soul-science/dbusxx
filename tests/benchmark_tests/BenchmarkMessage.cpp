//! Benchmarks for the message serialization layer: MessagePrivate write() /
//! read() and full write→seal→read round-trips over basic, compound and
//! large payloads. These are local, CPU-bound operations on an in-memory
//! sd-bus message buffer; a session-bus connection is needed only to own the
//! message object (the private daemon from BenchmarkMain.cpp provides it).
//!
//! A single `sd_bus*` is opened once per benchmark case and reused across
//! iterations — opening a fresh connection per message would swamp the
//! measured serialization cost.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "private/message/MessagePrivate.hpp"
#include "BenchmarkUtil.hpp"

using namespace Dbusxx;

namespace {

//! Aggregate structs mirroring the unit-test fixtures (must stay aggregates:
//! the library reflects field counts via aggregate construction).
struct Point {
    int32_t x;
    int32_t y;
};

struct Person {
    std::string name;
    int32_t     age;
};

struct Nested {
    Point       p;
    std::string tag;
};

//! Owns a session-bus connection for one benchmark case; also constructs
//! fresh MessagePrivate objects on that bus.
class BenchBus {
public:
    ~BenchBus() {
        if (mBus) {
            sd_bus_unref(mBus);
        }
    }

    bool init(benchmark::State& state) {
        mBus = BenchmarkUtil::makeBus();
        if (!mBus) {
            state.SkipWithError("no session bus available");
            return false;
        }

        return true;
    }

    Private::MessagePrivate newMessage() const {
        return BenchmarkUtil::makeMessage(mBus);
    }

private:
    sd_bus* mBus { nullptr };
};

//! Write-only micro-benchmark for a fixed value (each iteration builds a fresh
//! message so the append always starts from an empty buffer).
template <typename T>
void benchWrite(benchmark::State& state, const T& val) {
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        Status st = msg.write(val);
        benchmark::DoNotOptimize(st);
    }
}

//! Full write→seal→read round-trip: the realistic usage pattern for a value
//! passing through the library.
template <typename T>
void benchRoundTrip(benchmark::State& state, const T& val) {
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
        BenchmarkUtil::sealMessage(msg);
        T out {};
        benchmark::DoNotOptimize(msg.read(out));
        benchmark::DoNotOptimize(out);
    }
}

const char* kShortString = "hello, dbus";
const char* kMediumString = "The quick brown fox jumps over the lazy dog — D-Bus.";

} //! namespace

//! ---------------------------------------------------------------------------
//! Basic scalar types — write
//! ---------------------------------------------------------------------------
#define DBUSXX_BENCH_WRITE_BASIC(Name, Val)                              \
    static void write##Name(benchmark::State& state) {                   \
        benchWrite(state, Val);                                          \
    }                                                                    \
    BENCHMARK(write##Name)

DBUSXX_BENCH_WRITE_BASIC(Int8, int8_t(-7));
DBUSXX_BENCH_WRITE_BASIC(Uint8, uint8_t(200));
DBUSXX_BENCH_WRITE_BASIC(Int16, int16_t(-1234));
DBUSXX_BENCH_WRITE_BASIC(Uint16, uint16_t(54321));
DBUSXX_BENCH_WRITE_BASIC(Int32, int32_t(42));
DBUSXX_BENCH_WRITE_BASIC(Uint32, uint32_t(4294967295u));
DBUSXX_BENCH_WRITE_BASIC(Int64, INT64_C(-9223372036854775807) - 1);
DBUSXX_BENCH_WRITE_BASIC(Uint64, UINT64_C(18446744073709551615));
DBUSXX_BENCH_WRITE_BASIC(Double, 3.14159265358979);
DBUSXX_BENCH_WRITE_BASIC(Float, 3.5f);
DBUSXX_BENCH_WRITE_BASIC(Bool, true);
DBUSXX_BENCH_WRITE_BASIC(StringShort, std::string(kShortString));
DBUSXX_BENCH_WRITE_BASIC(StringMedium, std::string(kMediumString));
DBUSXX_BENCH_WRITE_BASIC(StringView, std::string_view(kShortString));

#undef DBUSXX_BENCH_WRITE_BASIC

//! ---------------------------------------------------------------------------
//! Basic scalar types — full round-trip
//! ---------------------------------------------------------------------------
#define DBUSXX_BENCH_RT_BASIC(Name, Val)                                 \
    static void roundTrip##Name(benchmark::State& state) {               \
        benchRoundTrip(state, Val);                                      \
    }                                                                    \
    BENCHMARK(roundTrip##Name)

DBUSXX_BENCH_RT_BASIC(Int32, int32_t(42));
DBUSXX_BENCH_RT_BASIC(Uint64, UINT64_C(18446744073709551615));
DBUSXX_BENCH_RT_BASIC(Double, 3.14159265358979);
DBUSXX_BENCH_RT_BASIC(Float, 3.5f);
DBUSXX_BENCH_RT_BASIC(Bool, true);
DBUSXX_BENCH_RT_BASIC(StringShort, std::string(kShortString));
DBUSXX_BENCH_RT_BASIC(StringMedium, std::string(kMediumString));

#undef DBUSXX_BENCH_RT_BASIC

//! ---------------------------------------------------------------------------
//! Compound types
//! ---------------------------------------------------------------------------

//! vector<int32_t> — write path with a small fixed payload. The round-trip
//! sibling below covers the full path; the range()-parameterised variants at
//! the bottom of the file sweep larger element counts.
static void writeVectorInt(benchmark::State& state) {
    const std::vector<int32_t> val{1, -2, 3, -4, 5};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
    }
}
BENCHMARK(writeVectorInt);

//! vector<int32_t> — full write→seal→read round-trip (fixed small payload).
static void roundTripVectorInt(benchmark::State& state) {
    const std::vector<int32_t> val{1, -2, 3, -4, 5};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
        BenchmarkUtil::sealMessage(msg);
        std::vector<int32_t> out;
        benchmark::DoNotOptimize(msg.read(out));
        benchmark::DoNotOptimize(out.size());
    }
}
BENCHMARK(roundTripVectorInt);

//! vector<std::string> — write path (element serialization dominates).
static void writeVectorString(benchmark::State& state) {
    const std::vector<std::string> val{"alpha", "beta", "gamma", "delta"};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
    }
}
BENCHMARK(writeVectorString);

//! std::map<std::string, int32_t> — write path (dict-entry serialization).
static void writeMapStringInt(benchmark::State& state) {
    const std::map<std::string, int32_t> val{
        {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
    }
}
BENCHMARK(writeMapStringInt);

//! std::map<std::string, int32_t> — full write→seal→read round-trip.
static void roundTripMapStringInt(benchmark::State& state) {
    const std::map<std::string, int32_t> val{
        {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
        BenchmarkUtil::sealMessage(msg);
        std::map<std::string, int32_t> out;
        benchmark::DoNotOptimize(msg.read(out));
        benchmark::DoNotOptimize(out.size());
    }
}
BENCHMARK(roundTripMapStringInt);

//! std::tuple — the multi-arg batch form.
static void writeTuple(benchmark::State& state) {
    const std::tuple<int32_t, std::string, bool> val{7, "seven", true};
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
    }
}
BENCHMARK(writeTuple);

//! Aggregate structs (reflected into (ii), (si), ((ii)s) signatures).
static void writeStructPoint(benchmark::State& state) {
    const Point val{1, 2};
    benchWrite(state, val);
}
BENCHMARK(writeStructPoint);

//! Aggregate struct Person (string + int32 → "(si)") round-trip.
static void roundTripStructPerson(benchmark::State& state) {
    const Person val{"alice", 30};
    benchRoundTrip(state, val);
}
BENCHMARK(roundTripStructPerson);

//! Nested aggregate (struct inside struct → "((ii)s)") round-trip.
static void roundTripStructNested(benchmark::State& state) {
    const Nested val{{1, 2}, "nested"};
    benchRoundTrip(state, val);
}
BENCHMARK(roundTripStructNested);

//! Multiple heterogeneous arguments packed in one write (common RPC shape).
static void roundTripMultiArg(benchmark::State& state) {
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(
            msg.write(int32_t(1), std::string("two"), double(3.5), bool(true)));
        BenchmarkUtil::sealMessage(msg);
        int32_t a = 0;
        std::string b;
        double c = 0.0;
        bool d = false;
        benchmark::DoNotOptimize(msg.read(a, b, c, d));
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        benchmark::DoNotOptimize(c);
        benchmark::DoNotOptimize(d);
    }
}
BENCHMARK(roundTripMultiArg);

//! ---------------------------------------------------------------------------
//! Large / scale-sensitive payloads (range()-parameterised)
//! ---------------------------------------------------------------------------

//! String write cost vs. length.
static void writeLargeString(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::string payload(n, 'x');
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(payload));
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(writeLargeString)
    ->RangeMultiplier(4)
    ->Range(16, 64 * 1024);

//! String round-trip cost vs. length.
static void roundTripLargeString(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::string payload(n, 'x');
    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(payload));
        BenchmarkUtil::sealMessage(msg);
        std::string out;
        benchmark::DoNotOptimize(msg.read(out));
        benchmark::DoNotOptimize(out.size());
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(roundTripLargeString)
    ->RangeMultiplier(4)
    ->Range(16, 64 * 1024);

//! vector<int32_t> write cost vs. element count.
static void writeLargeVector(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<int32_t> val(n);
    for (size_t i = 0; i < n; ++i) {
        val[i] = static_cast<int32_t>(i);
    }

    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(writeLargeVector)
    ->RangeMultiplier(10)
    ->Range(1, 10000);

//! vector<int32_t> full round-trip vs. element count.
static void roundTripLargeVector(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<int32_t> val(n);
    for (size_t i = 0; i < n; ++i) {
        val[i] = static_cast<int32_t>(i);
    }

    BenchBus bb;
    if (!bb.init(state)) {
        return;
    }

    for (auto _ : state) {
        auto msg = bb.newMessage();
        benchmark::DoNotOptimize(msg.write(val));
        BenchmarkUtil::sealMessage(msg);
        std::vector<int32_t> out;
        benchmark::DoNotOptimize(msg.read(out));
        benchmark::DoNotOptimize(out.size());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(roundTripLargeVector)
    ->RangeMultiplier(10)
    ->Range(1, 10000);
