//! Unit tests for PropertyWrapper get/set/onChanged/signature and the
//! onGetter/onSetter callbacks (library/include/private/method/Method.hpp).
//!
//! A default-constructed SessionPrivate has a null raw bus, so the
//! `sd_bus_emit_properties_changed` call inside set() returns -EINVAL
//! instead of crashing — this lets us exercise all the logic that does
//! not require a live connection. The message-based onGetter/onSetter
//! tests need a session D-Bus daemon and SKIP otherwise.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "private/session/SessionPrivate.hpp"
#include "private/method/Method.hpp"
#include "private/message/MessagePrivate.hpp"
#include "Args.hpp"
#include "Status.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;
using namespace Dbusxx::Method;

TEST(PropertyWrapperTest, ConstructAndGet) {
    Private::SessionPrivate session;   //! default: null raw bus
    PropertyWrapper<int32_t> pw(&session, "counter", "/test/path", "test.iface", 42);
    EXPECT_EQ(pw.get(), 42);
    EXPECT_STREQ(pw.propName.c_str(), "counter");
    EXPECT_STREQ(pw.propPath.c_str(), "/test/path");
    EXPECT_STREQ(pw.propIface.c_str(), "test.iface");
}

TEST(PropertyWrapperTest, SetSameValueShortCircuits) {
    Private::SessionPrivate session;
    PropertyWrapper<int32_t> pw(&session, "counter", "/p", "i", 5);
    int onChangeCalls = 0;
    pw.onChanged([&onChangeCalls](const int32_t&) { ++onChangeCalls; });

    pw.set(5);  //! equal → no update, no callback, no emit
    EXPECT_EQ(pw.get(), 5);
    EXPECT_EQ(onChangeCalls, 0);
}

TEST(PropertyWrapperTest, SetNewValueTriggersOnChange) {
    Private::SessionPrivate session;
    PropertyWrapper<int32_t> pw(&session, "counter", "/p", "i", 5);
    int32_t observed = 0;
    pw.onChanged([&observed](const int32_t& v) { observed = v; });

    pw.set(99);
    EXPECT_EQ(pw.get(), 99);
    EXPECT_EQ(observed, 99);
}

TEST(PropertyWrapperTest, OnChangedReplacePrevious) {
    Private::SessionPrivate session;
    PropertyWrapper<int32_t> pw(&session, "counter", "/p", "i", 1);
    int first = 0;
    int second = 0;
    pw.onChanged([&first](const int32_t&) { ++first; });
    pw.onChanged([&second](const int32_t&) { ++second; });  //! replaces first
    pw.set(2);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(PropertyWrapperTest, Signature) {
    EXPECT_STREQ(PropertyWrapper<int32_t>::signature().c_str(), "i");
    EXPECT_STREQ(PropertyWrapper<std::string>::signature().c_str(), "s");
    const std::string vecSig = PropertyWrapper<std::vector<int32_t>>::signature();
    EXPECT_STREQ(vecSig.c_str(), "ai");
    struct Pt { int32_t x; int32_t y; };
    EXPECT_STREQ(PropertyWrapper<Pt>::signature().c_str(), "(ii)");
}

TEST(PropertyWrapperTest, OnGetterWritesValue) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Private::SessionPrivate session;
    PropertyWrapper<int32_t> pw(&session, "counter", "/p", "i", 123);

    auto msg = Dbusxx::UnitTest::makeMessage();
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int ret = pw.onGetter(nullptr, "/p", "i", "counter",
                          msg.rawMessage(), &pw, &err);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(sd_bus_error_is_set(&err));

    //! The getter appended the value; seal it so it can be read back.
    Status st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());
    int32_t out = 0;
    st = msg.read(out);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_EQ(out, 123);
    sd_bus_error_free(&err);
}

TEST(PropertyWrapperTest, OnSetterSetsValue) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Private::SessionPrivate session;
    PropertyWrapper<int32_t> pw(&session, "counter", "/p", "i", 0);

    //! Feed a sealed message carrying the new value (onSetter reads it).
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(int32_t(777));
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());

    sd_bus_error err = SD_BUS_ERROR_NULL;
    int ret = pw.onSetter(nullptr, "/p", "i", "counter",
                          msg.rawMessage(), &pw, &err);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(sd_bus_error_is_set(&err));
    EXPECT_EQ(pw.get(), 777);
    sd_bus_error_free(&err);
}

TEST(PropertyWrapperTest, OnSetterTypeMismatch) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Private::SessionPrivate session;
    PropertyWrapper<std::string> pw(&session, "name", "/p", "i", "init");

    //! Feed a sealed message carrying an int32 — not a string → the setter
    //! must fail and fill aErr.
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(int32_t(1));
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());

    sd_bus_error err = SD_BUS_ERROR_NULL;
    int ret = pw.onSetter(nullptr, "/p", "i", "name",
                          msg.rawMessage(), &pw, &err);
    EXPECT_LT(ret, 0);
    EXPECT_TRUE(sd_bus_error_is_set(&err));
    EXPECT_STREQ(pw.get().c_str(), "init");  //! value unchanged
    sd_bus_error_free(&err);
}
