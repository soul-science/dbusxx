//! Unit tests for Session's direct API, without the Server/Client wrappers:
//! local property read/write and a raw register-then-call round-trip between
//! two bare Sessions (server side driven by a Looper).
//! Runs against the private dbus-daemon from main.cpp.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "Session.hpp"
#include "Looper.hpp"
#include "Reply.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;

TEST(SessionTest, LocalPropertyReadWrite) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no D-Bus daemon available";
    }

    Session s = Session::userSession();
    constexpr const char* path = "/com/test";
    constexpr const char* iface = "com.test.Iface";

    Status st = s.registerBuilder(path, iface)
                    .addProperty("counter", int32_t(0), true)
                    .commit();
    EXPECT_TRUE(st.isSuccess()) << st.message();

    //! Initial value.
    int32_t v = -1;
    st = s.getLocalProperty<int32_t>(path, iface, "counter", v);
    EXPECT_TRUE(st.isSuccess()) << st.message();
    EXPECT_EQ(v, 0);

    //! onChanged callback.
    int32_t observed = -1;
    st = s.onLocalPropertyChanged<int32_t>(
        path, iface, "counter",
        [&observed](const int32_t& nv) { observed = nv; });
    EXPECT_TRUE(st.isSuccess()) << st.message();

    //! Write a new value; read it back and confirm the callback fired.
    st = s.setLocalProperty(path, iface, "counter", int32_t(7));
    EXPECT_TRUE(st.isSuccess()) << st.message();

    st = s.getLocalProperty<int32_t>(path, iface, "counter", v);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_EQ(v, 7);
    EXPECT_EQ(observed, 7);

    //! Unknown property → error, value untouched.
    st = s.getLocalProperty<int32_t>(path, iface, "nope", v);
    EXPECT_TRUE(st.isError());
}

TEST(SessionTest, RegisterMethodAndCall) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no D-Bus daemon available";
    }

    constexpr const char* svc = "com.test.svc";
    constexpr const char* path = "/com/test";
    constexpr const char* iface = "com.test.Iface";

    //! Server side: bare Session + Looper serving a registered method.
    Session a = Session::userSession(svc);
    Status st = a.registerBuilder(path, iface)
                    .addMethod("echo", [](int32_t v) { return v; })
                    .commit();
    EXPECT_TRUE(st.isSuccess()) << st.message();

    Looper alooper(a);
    //! RAII: always stop() + join() the loop thread, even on early return
    //! (an ASSERT failure below would otherwise leave a joinable std::thread,
    //! which std::terminate()s at scope exit).
    struct ScopedLoop {
        Looper& loop;
        std::thread t;
        explicit ScopedLoop(Looper& l)
            : loop(l), t([&l] { l.run(); }) {}
        ~ScopedLoop() {
            loop.stop();
            if (t.joinable()) {
                t.join();
            }
        }
    } scopedLoop(alooper);

    //! Client side: another bare Session, plain blocking call.
    Session b = Session::userSession();
    const bool ready = Dbusxx::UnitTest::waitUntil([&] {
        auto r = b.callSync<int32_t>(svc, path, iface, "echo", 0);
        return !r.isError();
    });
    ASSERT_TRUE(ready) << "server side did not become ready";

    auto rep = b.callSync<int32_t>(svc, path, iface, "echo", 42);
    EXPECT_FALSE(rep.isError()) << rep.errorMessage();
    EXPECT_EQ(rep.value(), 42);
    //! scopedLoop destructor stops the loop and joins the thread.
}
