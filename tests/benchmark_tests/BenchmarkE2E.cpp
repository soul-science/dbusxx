//! End-to-end benchmarks: a real `Server<Derived>` on one thread plus a real
//! `Client` on the benchmark thread, talking through the private bus daemon.
//! Covers synchronous calls (scalar / compound / void / large payloads),
//! asynchronous calls, property get/set and signal emit→receive.
//!
//! Every measurement crosses threads and the daemon, so these are reported in
//! wall-clock time (`UseManualTime`); default CPU time would hide the blocked
//! waits and report misleadingly low numbers.

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Client.hpp"
#include "Reply.hpp"
#include "Server.hpp"
#include "BenchmarkUtil.hpp"

using namespace Dbusxx;

namespace {

//! Aggregate struct exercised over the wire (maps to (ii)).
struct Point {
    int32_t x;
    int32_t y;
};

//! Server under test: one method per payload shape plus a property and a
//! signal, all registered through the reflection macros.
class BenchServer : public Server<BenchServer> {
public:
    BenchServer() : Server("com.bench.svc") {}

    DBUSXX_PATH("/com/bench")
    DBUSXX_IFACE("com.bench.Iface")

    int32_t echoInt(int32_t v) { return v; }
    DBUSXX_METHOD(echoInt)

    std::string echoString(const std::string& s) { return s; }
    DBUSXX_METHOD(echoString)

    std::vector<int32_t> echoVec(const std::vector<int32_t>& v) { return v; }
    DBUSXX_METHOD(echoVec)

    Point echoStruct(Point p) { return p; }
    DBUSXX_METHOD(echoStruct)

    void echoVoid() {}
    DBUSXX_METHOD(echoVoid)

    std::string echoBig(const std::string& s) { return s; }
    DBUSXX_METHOD(echoBig)

    DBUSXX_PROPERTY_RW(counter, int32_t, 0)

    //! Emits the registered `tick` signal. Thread-safe: can be invoked from any thread.
    void fireTick(int32_t v) {
        benchmark::DoNotOptimize(
            emit("/com/bench", "com.bench.Iface", "tick", v));
    }
    DBUSXX_SIGNAL(tick, int32_t)
};

} //! namespace

//! Shared harness: server thread + client, reused across E2E benchmarks.
class E2EBench : public benchmark::Fixture {
public:
    //! Note: non-const State overload so SkipWithError() is available.
    void SetUp(benchmark::State& state) override {
        if (!BenchmarkUtil::busAvailable()) {
            state.SkipWithError("no session bus available");
            return;
        }

        mServer = std::make_unique<BenchServer>();
        mThread = std::thread([this] { mServer->run(); });

        mClient = std::make_unique<Client>(
            SessionType::USER, "com.bench.svc",
            "/com/bench", "com.bench.Iface");

        //! The server registers its name inside run(); wait until it answers.
        if (!BenchmarkUtil::waitUntil([this] {
                return !mClient->callSync<int32_t>("echoInt", 0).isError();
            })) {
            state.SkipWithError("server did not become ready in time");
            return;
        }

        //! Subscribe to `tick` once; a counter + condition variable let the
        //! signal benchmark wait for the *next* arrival each iteration.
        const Status sub = mClient->listenSignal("tick", [this](int32_t) {
            mSignalCount.fetch_add(1, std::memory_order_relaxed);
            mSignalCv.notify_all();
        });
        if (sub.isError()) {
            state.SkipWithError("failed to subscribe to tick signal");
            return;
        }
    }

    void TearDown(const benchmark::State&) override {
        if (mServer) {
            mServer->stop();
        }
        if (mThread.joinable()) {
            mThread.join();
        }
        mClient.reset();
        mServer.reset();
    }

    //! Run one wall-clock-timed iteration.
    template <typename F>
    void timedIteration(benchmark::State& state, F&& f) {
        auto start = std::chrono::steady_clock::now();
        f();
        state.SetIterationTime(
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count());
    }

protected:
    std::unique_ptr<BenchServer> mServer;
    std::unique_ptr<Client> mClient;
    std::thread mThread;

    std::atomic<int64_t> mSignalCount { 0 };
    std::mutex mSignalMtx;
    std::condition_variable mSignalCv;
};

//! ---------------------------------------------------------------------------
//! Synchronous method calls
//! ---------------------------------------------------------------------------

BENCHMARK_DEFINE_F(E2EBench, syncCallInt)(benchmark::State& state) {
    for (auto _ : state) {
        timedIteration(state, [this] {
            auto rep = mClient->callSync<int32_t>("echoInt", 42);
            benchmark::DoNotOptimize(rep.value());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, syncCallInt)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(E2EBench, syncCallVoid)(benchmark::State& state) {
    for (auto _ : state) {
        timedIteration(state, [this] {
            auto rep = mClient->callSync<void>("echoVoid");
            benchmark::DoNotOptimize(rep.status());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, syncCallVoid)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(E2EBench, syncCallString)(benchmark::State& state) {
    const std::string s = "hello, dbus — end to end";
    for (auto _ : state) {
        timedIteration(state, [this, &s] {
            auto rep = mClient->callSync<std::string>("echoString", s);
            benchmark::DoNotOptimize(rep.value());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, syncCallString)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(E2EBench, syncCallStruct)(benchmark::State& state) {
    const Point p{3, -4};
    for (auto _ : state) {
        timedIteration(state, [this, &p] {
            auto rep = mClient->callSync<Point>("echoStruct", p);
            benchmark::DoNotOptimize(rep.value());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, syncCallStruct)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(E2EBench, syncCallVector)(benchmark::State& state) {
    const std::vector<int32_t> v{1, -2, 3, -4, 5, 6};
    for (auto _ : state) {
        timedIteration(state, [this, &v] {
            auto rep = mClient->callSync<std::vector<int32_t>>("echoVec", v);
            benchmark::DoNotOptimize(rep.value().size());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, syncCallVector)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

//! Large-payload sync call: one RTT per payload size.
BENCHMARK_DEFINE_F(E2EBench, syncCallBigPayload)(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::string payload(n, 'x');
    for (auto _ : state) {
        timedIteration(state, [this, &payload] {
            auto rep = mClient->callSync<std::string>("echoBig", payload);
            benchmark::DoNotOptimize(rep.value().size());
        });
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK_REGISTER_F(E2EBench, syncCallBigPayload)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond)
    ->RangeMultiplier(4)
    ->Range(256, 128 * 1024);

//! ---------------------------------------------------------------------------
//! Asynchronous call (dispatch + wait on the returned handle)
//! ---------------------------------------------------------------------------

//! NB: `Client::callAsync<T>(method, arg)` is ambiguous between the
//! PendingReply form and the callback form (the latter wins overload
//! resolution for a plain second argument). Use the explicit callback form
//! and block on a promise — the underlying async dispatch is identical.
BENCHMARK_DEFINE_F(E2EBench, asyncCallInt)(benchmark::State& state) {
    for (auto _ : state) {
        timedIteration(state, [this] {
            std::promise<void> done;
            auto fut = done.get_future();
            const Status st = mClient->callAsync<int32_t>(
                "echoInt",
                [&done](Reply<int32_t>) { done.set_value(); },
                42);
            benchmark::DoNotOptimize(st);
            fut.wait();
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, asyncCallInt)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

//! ---------------------------------------------------------------------------
//! Properties
//! ---------------------------------------------------------------------------

BENCHMARK_DEFINE_F(E2EBench, propertyGet)(benchmark::State& state) {
    for (auto _ : state) {
        timedIteration(state, [this] {
            auto rep = mClient->getProperty<int32_t>("counter");
            benchmark::DoNotOptimize(rep.value());
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, propertyGet)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(E2EBench, propertySet)(benchmark::State& state) {
    for (auto _ : state) {
        timedIteration(state, [this] {
            Status st = mClient->setProperty("counter", 7);
            benchmark::DoNotOptimize(st);
        });
    }
}
BENCHMARK_REGISTER_F(E2EBench, propertySet)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

//! ---------------------------------------------------------------------------
//! Signal emit → receive round-trip
//! ---------------------------------------------------------------------------

BENCHMARK_DEFINE_F(E2EBench, signalEmitReceive)(benchmark::State& state) {
    //! One synchronous warm-up so the match rule is guaranteed live before the
    //! timed iterations. If the warm-up signal is missed the subscription is
    //! not yet live — report and bail out instead of burning 3s per iteration.
    {
        const int64_t before = mSignalCount.load(std::memory_order_relaxed);
        mServer->fireTick(0);
        std::unique_lock<std::mutex> lk(mSignalMtx);
        if (!mSignalCv.wait_for(lk, std::chrono::seconds(3), [&] {
                return mSignalCount.load(std::memory_order_relaxed) > before;
            })) {
            state.SkipWithError("signal subscription not live (warm-up missed)");
            return;
        }
    }

    for (auto _ : state) {
        const int64_t before = mSignalCount.load(std::memory_order_relaxed);
        auto start = std::chrono::steady_clock::now();

        mServer->fireTick(1);
        std::unique_lock<std::mutex> lk(mSignalMtx);
        if (!mSignalCv.wait_for(lk, std::chrono::seconds(3), [&] {
                return mSignalCount.load(std::memory_order_relaxed) > before;
            })) {
            state.SkipWithError("signal not received in time");
            return;
        }

        state.SetIterationTime(
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count());
    }
}
BENCHMARK_REGISTER_F(E2EBench, signalEmitReceive)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);
