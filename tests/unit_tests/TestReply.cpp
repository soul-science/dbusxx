//! Unit tests for Reply<Ret> (library/include/Reply.hpp).
//!
//! Payload parsing builds messages on a live session-bus connection (see
//! TestUtil.hpp) and therefore SKIPs when no daemon is present.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Message.hpp"
#include "Reply.hpp"
#include "private/message/MessagePrivate.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;

namespace {

//! Simple aggregate used as a struct payload.
struct Point {
    int32_t x;
    int32_t y;
};

//! Build a sealed message holding a single payload value (needs a bus).
template<typename T>
Private::MessagePrivate makePayloadMessage(const T& aValue) {
    auto msg = UnitTest::makeMessage();
    EXPECT_TRUE(msg.write(aValue).isSuccess());
    EXPECT_TRUE(UnitTest::sealMessage(msg).isSuccess());
    return msg;
}

} //! namespace

TEST(ReplyTest, EmptyReply) {
    Reply<int32_t> r;
    EXPECT_FALSE(r.isError());
    EXPECT_TRUE(r.status().isSuccess());
    EXPECT_EQ(r.value(), 0);
}

TEST(ReplyTest, ParsesScalarPayload) {
    if (!UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Reply<int32_t> r(std::make_shared<Private::MessagePrivate>(
        makePayloadMessage(int32_t(42))));
    EXPECT_FALSE(r.isError());
    EXPECT_TRUE(r.status().isSuccess());
    EXPECT_EQ(r.value(), 42);
}

TEST(ReplyTest, ParsesStringPayload) {
    if (!UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Reply<std::string> r(std::make_shared<Private::MessagePrivate>(
        makePayloadMessage(std::string("hello"))));
    EXPECT_FALSE(r.isError());
    EXPECT_STREQ(r.value().c_str(), "hello");
}

TEST(ReplyTest, ParsesVectorPayload) {
    if (!UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Reply<std::vector<int32_t>> r(std::make_shared<Private::MessagePrivate>(
        makePayloadMessage(std::vector<int32_t>{1, 2, 3})));
    EXPECT_FALSE(r.isError());
    ASSERT_EQ(r.value().size(), 3u);
    EXPECT_EQ(r.value()[0], 1);
    EXPECT_EQ(r.value()[2], 3);
}

TEST(ReplyTest, ParsesStructPayload) {
    if (!UnitTest::busAvailable()) {
        GTEST_SKIP() << "no session D-Bus daemon available";
    }

    Reply<Point> r(std::make_shared<Private::MessagePrivate>(
        makePayloadMessage(Point{7, 9})));
    EXPECT_FALSE(r.isError());
    EXPECT_EQ(r.value().x, 7);
    EXPECT_EQ(r.value().y, 9);
}

TEST(ReplyTest, EmptyImplementationIsError) {
    //! A Reply wrapping an implementation without a raw message cannot be
    //! parsed and must surface as an error rather than garbage.
    Reply<int32_t> r(std::make_shared<Private::MessagePrivate>());
    EXPECT_TRUE(r.isError());
}

TEST(ReplyTest, VoidReply) {
    Reply<void> r;
    EXPECT_FALSE(r.isError());
}
