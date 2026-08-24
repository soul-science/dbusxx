//! Unit tests for MessagePrivate read/write round-trips
//! (library/include/private/message/MessagePrivate.hpp).
//!
//! Messages are built on a live *session* bus connection (see TestUtil.hpp),
//! so a D-Bus daemon must be running; the tests SKIP when none is present.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "private/message/MessagePrivate.hpp"
#include "Args.hpp"
#include "Status.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;

namespace {

//! Aggregate structs with operator== (still aggregates: only a member fn).
struct Point {
    int32_t x;
    int32_t y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct Person {
    std::string name;
    int32_t age;
    bool operator==(const Person& o) const {
        return name == o.name && age == o.age;
    }
};

struct Nested {
    Point p;
    std::string tag;
    bool operator==(const Nested& o) const { return p == o.p && tag == o.tag; }
};

//! write → seal → read → compare
template<typename T>
void expectRoundTrip(const T& aValue) {
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(aValue);
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());
    T out {};
    st = msg.read(out);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_EQ(out, aValue);
}

} //! namespace

TEST(MessageTest, BasicRoundTrips) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    expectRoundTrip<int8_t>(static_cast<int8_t>(-7));
    expectRoundTrip<uint8_t>(static_cast<uint8_t>(200));
    expectRoundTrip<int16_t>(static_cast<int16_t>(-1234));
    expectRoundTrip<uint16_t>(static_cast<uint16_t>(54321));
    expectRoundTrip<int32_t>(42);
    expectRoundTrip<int32_t>(-42);
    expectRoundTrip<uint32_t>(4294967295u);
    expectRoundTrip<int64_t>(INT64_C(-9223372036854775807) - 1);
    expectRoundTrip<uint64_t>(UINT64_C(18446744073709551615));
    expectRoundTrip<double>(3.14159265358979);
    expectRoundTrip<float>(3.5f);          //! float → double → float, lossless
    expectRoundTrip<bool>(true);
    expectRoundTrip<bool>(false);
    expectRoundTrip<std::string>(std::string("hello, dbus"));
    expectRoundTrip<std::string>(std::string(""));   //! empty string

    //! write accepts string_view, but read only materializes std::string.
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(std::string_view("sv view"));
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());
    std::string sv;
    st = msg.read(sv);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_STREQ(sv.c_str(), "sv view");
}

TEST(MessageTest, MultiArgRoundTrip) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(int32_t(1), std::string("two"), double(3.5));
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());

    int32_t a = 0;
    std::string b;
    double c = 0.0;
    st = msg.read(a, b, c);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_EQ(a, 1);
    EXPECT_STREQ(b.c_str(), "two");
    EXPECT_EQ(c, 3.5);
}

TEST(MessageTest, TupleRoundTrip) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    std::tuple<int32_t, std::string, bool> in{7, "seven", true};
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(in);
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());

    std::tuple<int32_t, std::string, bool> out{};
    st = msg.read(out);
    EXPECT_TRUE(st.isSuccess());
    EXPECT_EQ(std::get<0>(out), 7);
    EXPECT_STREQ(std::get<1>(out).c_str(), "seven");
    EXPECT_EQ(std::get<2>(out), true);
}

TEST(MessageTest, VectorRoundTrips) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    expectRoundTrip<std::vector<int32_t>>({1, 2, 3});
    expectRoundTrip<std::vector<int32_t>>({});                        //! empty
    expectRoundTrip<std::vector<std::string>>({"a", "b", "c"});
    expectRoundTrip<std::vector<std::vector<int32_t>>>({{1, 2}, {3, 4, 5}});
    expectRoundTrip<std::vector<Point>>({{1, 2}, {3, 4}});
}

TEST(MessageTest, ArrayRoundTrips) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    expectRoundTrip<std::array<int32_t, 3>>({1, 2, 3});
    expectRoundTrip<std::array<double, 2>>({1.5, 2.5});
}

TEST(MessageTest, MapRoundTrips) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    expectRoundTrip<std::map<std::string, int32_t>>(
        {{"one", 1}, {"two", 2}});
    expectRoundTrip<std::map<std::string, int32_t>>({});              //! empty
    expectRoundTrip<std::map<int32_t, std::string>>({{1, "a"}, {2, "b"}});
    expectRoundTrip<std::map<std::string, std::vector<int32_t>>>(
        {{"k", {1, 2, 3}}});
    expectRoundTrip<std::unordered_map<int32_t, double>>({{1, 1.5}, {2, 2.5}});
}

TEST(MessageTest, StructRoundTrips) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    expectRoundTrip<Point>({10, 20});
    expectRoundTrip<Person>({"Alice", 30});
    expectRoundTrip<Nested>({{1, 2}, "tag"});
    expectRoundTrip<std::vector<Point>>({{1, 2}, {3, 4}});
    expectRoundTrip<std::map<std::string, Point>>({{"p", {5, 6}}});
}

TEST(MessageTest, ReadFromEmptyFails) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    auto msg = Dbusxx::UnitTest::makeMessage();
    //! Unsealed + no payload → read must fail rather than return garbage.
    int32_t v = 0;
    Status st = msg.read(v);
    EXPECT_TRUE(st.isError());
}

TEST(MessageTest, ArrayLengthMismatch) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    //! Write 3 elements, but read into a fixed-size array of 2 → INVALID_ARG.
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(std::vector<int32_t>{1, 2, 3});
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());
    std::array<int32_t, 2> out{};
    st = msg.read(out);
    EXPECT_EQ(st.code(), StatusCode::INVALID_ARG);
}

TEST(MessageTest, TypeMismatch) {
    if (!Dbusxx::UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    //! Write an int32 but read as a string → read must fail.
    auto msg = Dbusxx::UnitTest::makeMessage();
    Status st = msg.write(int32_t(42));
    EXPECT_TRUE(st.isSuccess());
    st = Dbusxx::UnitTest::sealMessage(msg);
    EXPECT_TRUE(st.isSuccess());
    std::string out;
    st = msg.read(out);
    EXPECT_TRUE(st.isError());
}
