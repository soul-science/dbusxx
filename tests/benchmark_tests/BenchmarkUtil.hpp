#ifndef DBUSXX_BENCHMARK_UTIL_HPP
#define DBUSXX_BENCHMARK_UTIL_HPP

//! Shared helpers for the Google Benchmark suite in tests/benchmark_tests/.
//! Provides a *private* D-Bus daemon harness (fully isolated from the system
//! bus) plus bus/message construction helpers used by the serialization and
//! end-to-end benchmarks. Self-contained on purpose: benchmarks must build
//! and run without any dependency on the unit-test tree.

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

namespace Dbusxx::BenchmarkUtil {

//! A private D-Bus daemon spawned as a child process, fully isolated from the
//! system bus. On start() it also overrides the DBUS_SESSION_BUS_ADDRESS
//! environment variable so every `userSession()` / `Client(USER, ...)` /
//! `sd_bus_open_user()` in this process connects to this private daemon.
//! Kills the child on destruction.
class BusDaemon {
public:
    BusDaemon() = default;
    BusDaemon(const BusDaemon&) = delete;
    BusDaemon& operator=(const BusDaemon&) = delete;
    BusDaemon(BusDaemon&&) = delete;
    BusDaemon& operator=(BusDaemon&&) = delete;

    ~BusDaemon() {
        if (mPid > 0) {
            ::kill(mPid, SIGTERM);
            ::waitpid(mPid, nullptr, 0);
        }
    }

    //! Spawn `/usr/bin/dbus-daemon` with a private session bus. Returns false
    //! if the binary is missing or the address can't be read. Idempotent.
    bool start() {
        if (mPid > 0) {
            return true;
        }

        int fds[2];
        if (::pipe(fds) != 0) {
            std::fprintf(stderr,
                "[dbusxx benchmarks] BusDaemon: pipe() failed: %s\n",
                std::strerror(errno));
            return false;
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            std::fprintf(stderr,
                "[dbusxx benchmarks] BusDaemon: fork() failed: %s\n",
                std::strerror(errno));
            ::close(fds[0]);
            ::close(fds[1]);
            return false;
        }
        if (pid == 0) {
            //! Child: ask the kernel to SIGTERM us when the parent dies.
            const pid_t ppid = ::getppid();
            ::prctl(PR_SET_PDEATHSIG, SIGTERM);
            if (::getppid() != ppid) {
                ::_exit(127);
            }

            ::dup2(fds[1], STDOUT_FILENO);
            ::close(fds[0]);
            ::close(fds[1]);
            ::execl("/usr/bin/dbus-daemon", "dbus-daemon",
                    "--session", "--nofork",
                    "--print-address=1", "--print-pid=1",
                    static_cast<char*>(nullptr));
            //! stderr is untouched in the child, so this shows up in the log
            //! when /usr/bin/dbus-daemon is missing or not executable.
            std::fprintf(stderr,
                "[dbusxx benchmarks] BusDaemon child: execv(/usr/bin/dbus-daemon) failed: %s\n",
                std::strerror(errno));
            ::_exit(127);
        }

        ::close(fds[1]);

        //! The daemon prints the address and pid on separate lines, which can
        //! arrive in separate pipe reads (especially under load) — a single
        //! read() may return only the address line, leaving pidStr empty and
        //! failing the parse (seen as a spurious "no private daemon"). Keep
        //! reading until both lines are seen, with a poll() timeout so a
        //! stuck daemon can't hang us forever.
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

                //! Re-parse whatever we have accumulated so far.
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

                //! Wait for more data; bounded by poll's timeout, and the
                //! outer deadline guards the whole loop (no spin, no hang).
                const int r = ::poll(&pfd, 1, 200);
                if (r < 0 && errno != EINTR) {
                    break;
                }
            }
        }
        ::close(fds[0]);

        if (addr.empty() || pidStr.empty()) {
            std::fprintf(stderr,
                "[dbusxx benchmarks] BusDaemon: cannot parse daemon output: '%s'"
                " (addr=%s pid=%s)\n",
                collected.c_str(),
                addr.empty() ? "<none>" : addr.c_str(),
                pidStr.empty() ? "<none>" : pidStr.c_str());
            ::kill(pid, SIGKILL);
            ::waitpid(pid, nullptr, 0);
            return false;
        }

        mAddress = addr;
        mPid = std::atoi(pidStr.c_str());
        ::setenv("DBUS_SESSION_BUS_ADDRESS", mAddress.c_str(), 1);
        return true;
    }

    const std::string& address() const { return mAddress; }

private:
    pid_t mPid { -1 };
    std::string mAddress;
};

//! Connect to the session bus (pointed at the private daemon by BusDaemon).
//! Returns nullptr on failure.
inline sd_bus* makeBus() {
    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0) {
        return nullptr;
    }
    return bus;
}

//! True when a session D-Bus daemon is reachable. Detected once and cached.
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

//! Create an empty signal message on a *persistent* bus (caller keeps owning
//! `aBus`). Reusing one bus across iterations is essential: opening a fresh
//! connection per message would dominate the measured write/read cost.
inline Dbusxx::Private::MessagePrivate makeMessage(sd_bus* aBus) {
    sd_bus_message* raw = nullptr;
    if (aBus &&
        sd_bus_message_new_signal(
            aBus, &raw, "/bench/path", "bench.iface", "BenchMethod") >= 0 &&
        raw != nullptr) {
        return Dbusxx::Private::MessagePrivate(
            Dbusxx::Adaptor::RawMessageSharePtr(raw, true));
    }

    return Dbusxx::Private::MessagePrivate(
        Dbusxx::Adaptor::RawMessageSharePtr(nullptr, false));
}

//! Seal a message so its payload becomes readable (sd-bus refuses to read an
//! unsealed message). Must be called between write() and read().
inline Dbusxx::Status sealMessage(Dbusxx::Private::MessagePrivate& aMsg) {
    return Dbusxx::Adaptor::RawErrorConvert::makeStatus(
        sd_bus_message_seal(aMsg.rawMessage(), 1, 0));
}

//! Poll `f()` until it returns true or the deadline passes (no fixed sleeps).
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

} //! namespace Dbusxx::BenchmarkUtil

#endif //! DBUSXX_BENCHMARK_UTIL_HPP
