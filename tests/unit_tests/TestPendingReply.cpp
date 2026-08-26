//! Unit tests for PendingReply<Ret> (library/include/PendingReply.hpp).
//!
//! Delivery logic (synchronous error delivery, wait/waitFor, late-installed
//! callbacks) is driven directly via ReplyAsyncHandler and needs no bus.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "PendingReply.hpp"
#include "Reply.hpp"
#include "private/message/ReplyAsyncHandler.hpp"

using namespace Dbusxx;

namespace {

//! A handler that has already completed with an error (mimics the
//! write-failure / callAsync-failure path in Method::callAsync).
std::shared_ptr<Private::ReplyAsyncHandler> makeFailedHandler() {
    auto h = std::make_shared<Private::ReplyAsyncHandler>();
    h->setStatus(Status(StatusCode::INVALID_ARG));
    h->isFinished = true;
    return h;
}

} //! namespace

TEST(PendingReplyTest, EmptyHandleIsInvalid) {
    PendingReply<int32_t> p;
    EXPECT_TRUE(p.getStatus().isError());   //! INVALID_ARG
    EXPECT_FALSE(p.isError());              //! no handler attached
    EXPECT_EQ(p.errorMessage(), std::string());
}

TEST(PendingReplyTest, SynchronousErrorDelivery) {
    PendingReply<int32_t> p(makeFailedHandler());

    //! Status is visible immediately (no bus round-trip needed).
    EXPECT_TRUE(p.getStatus().isError());
    EXPECT_TRUE(p.isError());

    //! wait() must not block: the future is already ready.
    p.wait();
    EXPECT_TRUE(p.reply().isError());

    //! waitFor() reports ready (not a timeout).
    EXPECT_TRUE(p.waitFor(10));

    //! A callback installed after the synchronous failure still fires once
    //! (the late-install delivery path added for the write-failure fix).
    int calls = 0;
    Reply<int32_t> got;
    p.setCallback([&](Reply<int32_t> r) {
        ++calls;
        got = r;
    });
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(got.isError());
}

TEST(PendingReplyTest, WaitForTimesOutWhileInFlight) {
    PendingReply<int32_t> p(std::make_shared<Private::ReplyAsyncHandler>());
    EXPECT_FALSE(p.waitFor(5));   //! no reply within 5 ms
}

TEST(PendingReplyTest, DeliverAfterTimeout) {
    auto h = std::make_shared<Private::ReplyAsyncHandler>();
    PendingReply<int32_t> p(h);

    EXPECT_FALSE(p.waitFor(5));

    //! Simulate the reply arriving (what ReplyAsyncHandler::onReply does):
    //! invoke the internal delivery callback, then mark the handler finished.
    ASSERT_TRUE(static_cast<bool>(h->mCallback));
    h->mCallback(h.get());
    h->isFinished = true;

    EXPECT_TRUE(p.waitFor(100));
    //! No payload message was attached, so the delivered reply is an error.
    EXPECT_TRUE(p.reply().isError());
}

TEST(PendingReplyTest, VoidSpecialization) {
    PendingReply<void> p(makeFailedHandler());
    EXPECT_TRUE(p.getStatus().isError());
    EXPECT_TRUE(p.waitFor(10));

    bool fired = false;
    p.setCallback([&](Reply<void>) { fired = true; });
    EXPECT_TRUE(fired);
}
