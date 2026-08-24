//! Custom test entry point (replaces GTest::gtest_main).
//!
//! Starts a *private* dbus-daemon (when /usr/bin/dbus-daemon exists) and
//! points DBUS_SESSION_BUS_ADDRESS at it, so business-layer tests
//! (Server/Client) run against an isolated bus. Falls back to the system
//! session bus if the private daemon cannot be spawned. The banner makes
//! the chosen bus visible in the test output.

#include <gtest/gtest.h>

#include <cstdio>

#include "TestUtil.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    //! Owned for the whole test run; killed when main() returns.
    static Dbusxx::UnitTest::TestBusDaemon privateDaemon;
    const bool ownDaemon = privateDaemon.start();
    const bool bus = Dbusxx::UnitTest::busAvailable();

    if (ownDaemon) {
        std::printf("[dbusxx unit tests] private bus daemon: %s\n",
                    privateDaemon.address().c_str());
    } else {
        std::printf("[dbusxx unit tests] no private daemon; session bus: %s\n",
                    bus ? "available (system)"
                        : "NOT available (daemon-dependent tests will SKIP)");
    }

    return RUN_ALL_TESTS();
}
