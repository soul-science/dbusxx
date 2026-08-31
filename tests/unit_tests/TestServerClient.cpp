//! Business-layer end-to-end tests against a *private* dbus-daemon.
//!
//! The private daemon is spawned by main.cpp (TestBusDaemon) and
//! DBUS_SESSION_BUS_ADDRESS points at it, so these tests exercise the real
//! Server<Derived> + Client stack in a fully isolated environment — no
//! dependency on the system bus. If no daemon at all is reachable they SKIP.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Server.hpp"
#include "Client.hpp"
#include "Reply.hpp"
#include "UnixFd.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;

namespace {

class TestServer : public Server<TestServer> {
public:
    TestServer() : Server("com.example.svc") {}

    DBUSXX_PATH("/com/example")
    DBUSXX_IFACE("com.example.Iface")

    int32_t echoInt(int32_t v) { return v; }
    DBUSXX_METHOD(echoInt)

    std::string echoString(const std::string& s) { return s; }
    DBUSXX_METHOD(echoString)

    UnixFd echoFd(UnixFd fd) { return fd; }
    DBUSXX_METHOD(echoFd)

    std::vector<UnixFd> echoFdList(std::vector<UnixFd> fds) { return fds; }
    DBUSXX_METHOD(echoFdList)

    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
};

class BusDaemonE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Dbusxx::UnitTest::busAvailable()) {
            GTEST_SKIP() << "no D-Bus daemon available";
        }

        mServer = std::make_unique<TestServer>();
        mThread = std::thread([this] { mServer->run(); });

        mClient = std::make_unique<Client>(
            SessionType::USER, "com.example.svc",
            "/com/example", "com.example.Iface");

        //! The server registers its name inside run(); wait until it answers.
        ASSERT_TRUE(Dbusxx::UnitTest::waitUntil([this] {
            return !mClient->callSync<int32_t>("echoInt", 0).isError();
        })) << "server did not become ready in time";
    }

    void TearDown() override {
        if (mServer) {
            mServer->stop();
        }

        if (mThread.joinable()) {
            mThread.join();
        }

        mClient.reset();
        mServer.reset();
    }

    std::unique_ptr<TestServer> mServer;
    std::unique_ptr<Client> mClient;
    std::thread mThread;
};

} //! namespace

TEST_F(BusDaemonE2ETest, EchoInt) {
    auto rep = mClient->callSync<int32_t>("echoInt", 42);
    EXPECT_FALSE(rep.isError()) << rep.errorMessage();
    EXPECT_EQ(rep.value(), 42);

    auto repNeg = mClient->callSync<int32_t>("echoInt", -7);
    EXPECT_FALSE(repNeg.isError());
    EXPECT_EQ(repNeg.value(), -7);
}

TEST_F(BusDaemonE2ETest, EchoString) {
    auto rep = mClient->callSync<std::string>(
        "echoString", std::string("hello, dbus"));
    EXPECT_FALSE(rep.isError()) << rep.errorMessage();
    EXPECT_STREQ(rep.value().c_str(), "hello, dbus");
}

TEST_F(BusDaemonE2ETest, PropertyReadWrite) {
    //! Initial value from the macro.
    auto init = mClient->getProperty<int32_t>("counter");
    EXPECT_FALSE(init.isError()) << init.errorMessage();
    EXPECT_EQ(init.value(), 0);

    //! Set then read back.
    Status st = mClient->setProperty("counter", 7);
    EXPECT_TRUE(st.isSuccess()) << st.message();

    auto after = mClient->getProperty<int32_t>("counter");
    EXPECT_FALSE(after.isError()) << after.errorMessage();
    EXPECT_EQ(after.value(), 7);
}

TEST_F(BusDaemonE2ETest, UnixFdEcho) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);

    auto rep = mClient->callSync<UnixFd>("echoFd", UnixFd(fds[0]));
    ASSERT_FALSE(rep.isError()) << rep.errorMessage();
    EXPECT_GE(rep.value().get(), 0);

    //! Functional round-trip through the real daemon (SCM_RIGHTS).
    ASSERT_EQ(::write(fds[1], "hello", 5), 5);
    char buf[8] = {0};
    EXPECT_EQ(::read(rep.value().get(), buf, 5), 5);
    EXPECT_STREQ(buf, "hello");

    ::close(fds[1]);
}

TEST_F(BusDaemonE2ETest, UnixFdListEcho) {
    int pa[2], pb[2];
    ASSERT_EQ(::pipe(pa), 0);
    ASSERT_EQ(::pipe(pb), 0);
    ::fcntl(pa[0], F_SETFL, ::fcntl(pa[0], F_GETFL, 0) | O_NONBLOCK);
    ::fcntl(pb[0], F_SETFL, ::fcntl(pb[0], F_GETFL, 0) | O_NONBLOCK);

    std::vector<UnixFd> in;
    in.push_back(UnixFd(pa[0]));
    in.push_back(UnixFd(pb[0]));
    auto rep = mClient->callSync<std::vector<UnixFd>>("echoFdList", in);
    ASSERT_FALSE(rep.isError()) << rep.errorMessage();
    ASSERT_EQ(rep.value().size(), 2u);
    EXPECT_GE(rep.value()[0].get(), 0);
    EXPECT_GE(rep.value()[1].get(), 0);
    EXPECT_NE(rep.value()[0].get(), rep.value()[1].get());

    ASSERT_EQ(::write(pa[1], "AB", 2), 2);
    char buf[4] = {0};
    EXPECT_EQ(::read(rep.value()[0].get(), buf, 2), 2);
    EXPECT_STREQ(buf, "AB");

    ::close(pa[1]);
    ::close(pb[1]);
}
