//! Benchmark entry point (replaces benchmark_main from Google Benchmark).
//!
//! Starts a *private* dbus-daemon (when /usr/bin/dbus-daemon exists) and
//! points DBUS_SESSION_BUS_ADDRESS at it, so the serialization and E2E
//! benchmarks run against an isolated bus — no system D-Bus daemon required.
//! Pure-logic benchmarks (Status) work regardless.

#include <benchmark/benchmark.h>

#include <cstdio>

#include "BenchmarkUtil.hpp"

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);

    //! Owned for the whole benchmark run; killed when main() returns.
    static Dbusxx::BenchmarkUtil::BusDaemon privateDaemon;
    const bool ownDaemon = privateDaemon.start();
    const bool bus = Dbusxx::BenchmarkUtil::busAvailable();

    if (ownDaemon) {
        std::printf("[dbusxx benchmarks] private bus daemon: %s\n",
                    privateDaemon.address().c_str());
    } else {
        std::printf("[dbusxx benchmarks] no private daemon; session bus: %s\n",
                    bus ? "available (system)"
                        : "NOT available (daemon-dependent benchmarks will SKIP)");
    }

    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
