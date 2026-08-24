//! Unit tests for the Status layer (library/include/Status.hpp).
//! Pure value type — no D-Bus daemon needed.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Status.hpp"

using namespace Dbusxx;

namespace {

const std::vector<StatusCode>& allCodes() {
    static const std::vector<StatusCode> codes = {
        StatusCode::SUCCESS,
        StatusCode::INVALID_ARG, StatusCode::NOT_FOUND, StatusCode::NO_SERVICE,
        StatusCode::NO_METHOD, StatusCode::ACCESS_DENIED, StatusCode::NAME_EXISTS,
        StatusCode::NOT_CONNECTED, StatusCode::CONN_RESET, StatusCode::BUSY,
        StatusCode::TIMEOUT, StatusCode::NO_MEMORY, StatusCode::NO_REPLY,
        StatusCode::IO_ERROR, StatusCode::MSG_TOO_LONG, StatusCode::LIMIT_EXCEEDED,
        StatusCode::PROTOCOL_ERROR, StatusCode::TYPE_MISMATCH, StatusCode::DISCONNECTED,
        StatusCode::UNKNOWN_ERROR,
    };
    return codes;
}

} //! namespace

TEST(StatusTest, DefaultConstruct) {
    Status st;
    EXPECT_TRUE(st.isSuccess());
    EXPECT_FALSE(st.isError());
    EXPECT_EQ(st.code(), StatusCode::SUCCESS);
}

TEST(StatusTest, ExplicitConstruct) {
    for (StatusCode c : allCodes()) {
        Status st(c);
        EXPECT_EQ(st.code(), c);
        if (c == StatusCode::SUCCESS) {
            EXPECT_TRUE(st.isSuccess());
            EXPECT_FALSE(st.isError());
        } else {
            EXPECT_FALSE(st.isSuccess());
            EXPECT_TRUE(st.isError());
        }
    }
}

TEST(StatusTest, StatusMessageCoverage) {
    //! Every code must map to a non-empty, distinct description.
    std::vector<std::string> messages;
    for (StatusCode c : allCodes()) {
        std::string m = statusMessage(c);
        EXPECT_FALSE(m.empty());
        for (const auto& prev : messages) {
            EXPECT_NE(m, prev);
        }
        messages.push_back(m);
    }

    //! Fallback for out-of-range / invalid value.
    StatusCode bogus = static_cast<StatusCode>(0xFF);
    EXPECT_STREQ(statusMessage(bogus), "Unknown");
}

TEST(StatusTest, CopyAndMove) {
    Status a(StatusCode::TIMEOUT);
    Status b(a);                 //! copy ctor
    EXPECT_EQ(b.code(), StatusCode::TIMEOUT);

    Status c = a;                //! copy assign
    EXPECT_EQ(c.code(), StatusCode::TIMEOUT);

    Status d(std::move(a));      //! move ctor
    EXPECT_EQ(d.code(), StatusCode::TIMEOUT);

    Status e;
    e = std::move(d);            //! move assign
    EXPECT_EQ(e.code(), StatusCode::TIMEOUT);

    //! Source after move is unspecified but must remain a valid Status.
    EXPECT_TRUE(a.isSuccess() || a.isError());
}

TEST(StatusTest, Message) {
    Status st(StatusCode::IO_ERROR);
    EXPECT_STREQ(st.message().c_str(), "I/O error");
    EXPECT_STREQ(Status(StatusCode::SUCCESS).message().c_str(), "Success");
}
