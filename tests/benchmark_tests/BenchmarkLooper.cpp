//! Benchmarks for the session/looper layer: connection lifecycle cost and
//! cross-thread task dispatch through the event loop. Requires the private
//! bus daemon from BenchmarkMain.cpp; benchmarks SKIP when none is present.

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "Looper.hpp"
#include "Session.hpp"
#include "BenchmarkUtil.hpp"

using namespace Dbusxx;

namespace {

//! Session construction + teardown (connection to the bus daemon). This is
//! one of the most expensive operations users hit, so it is parameterised by
//! whether a well-known service name is also requested.
static void sessionCreate(benchmark::State& state) {
    if (!BenchmarkUtil::busAvailable()) {
        state.SkipWithError("no session bus available");
        return;
    }

    const bool withName = state.range(0) != 0;
    for (auto _ : state) {
        Session s = withName ? Session::userSession("bench.svc.create")
                             : Session::userSession();
        benchmark::DoNotOptimize(s.type());
    }
}
BENCHMARK(sessionCreate)
    ->Arg(0)
    ->Arg(1)
    ->ArgName("requestName");

} //! namespace

//! Looper task-dispatch fixture: a session driven by a looper on a worker
//! thread, with the benchmark thread posting tasks into it — exactly how
//! `Client::callSync` shuttles work to its async thread.
class LooperDispatchBench : public benchmark::Fixture {
public:
    //! Note: non-const State overload so SkipWithError() is available.
    void SetUp(benchmark::State& state) override {
        if (!BenchmarkUtil::busAvailable()) {
            state.SkipWithError("no session bus available");
            return;
        }

        mSession = std::make_unique<Session>(Session::userSession());
        mLooper = std::make_unique<Looper>(*mSession);
        mThread = std::thread([this] { mLooper->run(); });

        //! Wait until the loop is dispatching (status flips to success).
        if (!BenchmarkUtil::waitUntil(
                [this] { return mLooper->status().isSuccess(); }, 3000)) {
            state.SkipWithError("looper failed to start");
            return;
        }
    }

    void TearDown(const benchmark::State&) override {
        if (mLooper) {
            mLooper->stop();
        }

        if (mThread.joinable()) {
            mThread.join();
        }

        mLooper.reset();
        mSession.reset();
    }

protected:
    std::unique_ptr<Session> mSession;
    std::unique_ptr<Looper> mLooper;
    std::thread mThread;
};

//! One post → execute → ack round-trip across the loop thread. Wall-clock
//! (the loop runs on another thread, so CPU time would hide the latency).
BENCHMARK_DEFINE_F(LooperDispatchBench, singleTaskLatency)(benchmark::State& state) {
    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();
        std::promise<void> done;
        auto fut = done.get_future();
        mLooper->post([&done] { done.set_value(); });
        fut.get();
        state.SetIterationTime(
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    }
}
BENCHMARK_REGISTER_F(LooperDispatchBench, singleTaskLatency)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

//! Throughput: post `n` tasks in a batch, wait for all of them, repeat.
//! SetItemsProcessed makes the report show tasks/second.
BENCHMARK_DEFINE_F(LooperDispatchBench, batchThroughput)(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    std::atomic<int> remaining { 0 };

    for (auto _ : state) {
        remaining.store(n);
        std::promise<void> allDone;
        auto fut = allDone.get_future();

        for (int i = 0; i < n; ++i) {
            mLooper->post([&remaining, &allDone] {
                if (remaining.fetch_sub(1) == 1) {
                    allDone.set_value();
                }
            });
        }

        fut.get();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK_REGISTER_F(LooperDispatchBench, batchThroughput)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->ArgName("tasks/batch");
