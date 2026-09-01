#ifndef DBUSXX_TEST_UTIL_HPP
#define DBUSXX_TEST_UTIL_HPP

//! Shared helpers for the Google Test based unit-test suite in
//! tests/unit_tests/. Assertions come from <gtest/gtest.h> in each test
//! file; this header provides bus/message construction helpers plus a
//! private D-Bus daemon harness for business-layer (Server/Client) tests.

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

#include "private/adaptor/RawMessageSharePtr.hpp"
#include "private/message/MessagePrivate.hpp"

namespace Dbusxx::UnitTest {

//! A private D-Bus daemon spawned as a child process, fully isolated from
//! the system bus. On start() it also overrides the DBUS_SESSION_BUS_ADDRESS
//! environment variable so every `userSession()` / `Client(USER, ...)` /
//! `sd_bus_open_user()` in this process connects to this private daemon.
//! Kills the child on destruction.
class TestBusDaemon {
public:
    TestBusDaemon() = default;
    //! Owns a child process — not copyable/movable (two owners would both
    //! kill/wait the same pid).
    TestBusDaemon(const TestBusDaemon&) = delete;
    TestBusDaemon& operator=(const TestBusDaemon&) = delete;
    TestBusDaemon(TestBusDaemon&&) = delete;
    TestBusDaemon& operator=(TestBusDaemon&&) = delete;

    ~TestBusDaemon() {
        if (mPid > 0) {
            ::kill(mPid, SIGTERM);
            ::waitpid(mPid, nullptr, 0);
        }
    }

    //! Spawn `/usr/bin/dbus-daemon` with a private session bus.
    //! Returns false if the binary is missing or the address can't be read.
    //! Idempotent: a second call returns true without spawning a new child.
    bool start() {
        if (mPid > 0) {
            return true;
        }

        int fds[2];
        if (::pipe(fds) != 0) {
            return false;
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            ::close(fds[0]);
            ::close(fds[1]);
            return false;
        }
        if (pid == 0) {
            //! Child: ask the kernel to SIGTERM us when the parent (the test
            //! process) dies — no matter how it dies (normal exit, crash,
            //! SIGKILL). This guarantees no orphaned private daemon is left
            //! behind even if the test runner is killed mid-run.
            const pid_t ppid = ::getppid();
            ::prctl(PR_SET_PDEATHSIG, SIGTERM);
            if (::getppid() != ppid) {
                //! Race: the parent died before PDEATHSIG took effect.
                ::_exit(127);
            }

            //! Send the daemon's address/pid to our pipe, then exec.
            ::dup2(fds[1], STDOUT_FILENO);
            ::close(fds[0]);
            ::close(fds[1]);
            ::execl("/usr/bin/dbus-daemon", "dbus-daemon",
                    "--session", "--nofork",
                    "--print-address=1", "--print-pid=1",
                    static_cast<char*>(nullptr));
            ::_exit(127);
        }

        ::close(fds[1]);

        //! The daemon prints the address and pid on separate lines, which can
        //! arrive in separate pipe reads (especially under load) — a single
        //! read() may return only the address line and fail the parse. Keep
        //! reading until both lines are seen (poll() timeout guards a hang).
        std::string collected;
        std::string addr;
        std::string pidStr;
        {
            //! Read non-blocking so poll() can actually enforce the deadline:
            //! a blocking read() would bypass the outer 5s timeout if the
            //! daemon wrote the address line but then stalled (no pid line).
            const int fl = ::fcntl(fds[0], F_GETFL, 0);
            if (fl >= 0) {
                ::fcntl(fds[0], F_SETFL, fl | O_NONBLOCK);
            }

            struct pollfd pfd = { fds[0], POLLIN, 0 };
            char buf[4096];
            const auto deadline = std::chrono::steady_clock::now()
                                  + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                const ssize_t n = ::read(fds[0], buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    collected += buf;
                } else if (n == 0) {
                    break;  //! daemon closed stdout (e.g. exec failed)
                } else {
                    const int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        break;
                    }
                }

                addr.clear();
                pidStr.clear();
                std::istringstream iss(collected);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) {
                        continue;
                    }
                    if (line.compare(0, 5, "unix:") == 0) {
                        addr = line;
                    } else if (std::isdigit(static_cast<unsigned char>(line[0]))) {
                        pidStr = line;
                    }
                }
                if (!addr.empty() && !pidStr.empty()) {
                    break;
                }

                const int r = ::poll(&pfd, 1, 200);
                if (r < 0 && errno != EINTR) {
                    break;
                }
            }
        }
        ::close(fds[0]);

        if (addr.empty() || pidStr.empty()) {
            ::kill(pid, SIGKILL);
            ::waitpid(pid, nullptr, 0);
            return false;
        }

        mAddress = addr;
        mPid = std::atoi(pidStr.c_str());
        //! Point every session-bus connection in this process at our daemon.
        ::setenv("DBUS_SESSION_BUS_ADDRESS", mAddress.c_str(), 1);
        return true;
    }

    const std::string& address() const { return mAddress; }

private:
    pid_t mPid { -1 };
    std::string mAddress;
};

//! Connect to the *session* bus. Messages can be built on it and
//! round-tripped locally without any service; a D-Bus daemon must be
//! running (usual on dev boxes / WSL). Guard with `busAvailable()` and
//! skip when no daemon is present.
inline sd_bus* makeTestBus() {
    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0) {
        return nullptr;
    }
    return bus;
}

//! True when a session D-Bus daemon is reachable (tests should SKIP else).
//! Detected once and cached: the custom main() prints a banner using this,
//! and every daemon-dependent test reuses the same answer.
inline bool busAvailable() {
    static const bool available = [] {
        sd_bus* bus = nullptr;
        if (sd_bus_open_user(&bus) < 0) {
            return false;
        }
        sd_bus_unref(bus);
        return true;
    }();
    return available;
}

//! Create an empty signal message for read/write round-trips.
//! The returned MessagePrivate owns the raw message (unref on destroy);
//! the local bus reference is dropped here because the message already
//! holds its own reference to the bus.
inline Dbusxx::Private::MessagePrivate makeMessage() {
    sd_bus* bus = makeTestBus();
    sd_bus_message* raw = nullptr;
    if (bus) {
        sd_bus_message_new_signal(
            bus, &raw, "/test/path", "test.iface", "TestMethod");
        sd_bus_unref(bus);
    }
    return Dbusxx::Private::MessagePrivate(
        Dbusxx::Adaptor::RawMessageSharePtr(raw, true));
}

//! Seal a message so its payload becomes readable (sd-bus refuses to read
//! an unsealed message). Must be called between write() and read().
inline Dbusxx::Status sealMessage(Dbusxx::Private::MessagePrivate& aMsg) {
    return Dbusxx::Adaptor::RawErrorConvert::makeStatus(
        sd_bus_message_seal(aMsg.rawMessage(), 1, 0));
}

//! Poll `f()` until it returns true or the deadline passes (no fixed sleeps).
//! Shared by the async/thread-based tests (looper readiness, server startup...).
template<typename F>
inline bool waitUntil(F&& f, int aTimeoutMs = 3000) {
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(aTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (f()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return f();
}

} //! namespace Dbusxx::UnitTest

#endif //! DBUSXX_TEST_UTIL_HPP
