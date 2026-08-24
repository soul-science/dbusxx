//! Unit tests for Looper's standalone capabilities: post() cross-thread
//! delivery, isOwnerThread(), task ordering and stop().
//!
//! The loop is bound to a plain user-session connection (the private daemon
//! started in main.cpp); no remote service is involved.

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Session.hpp"
#include "Looper.hpp"
#include "TestUtil.hpp"

using namespace Dbusxx;

class LooperTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Dbusxx::UnitTest::busAvailable()) {
            GTEST_SKIP() << "no D-Bus daemon available";
        }

        mSession = std::make_unique<Session>(Session::userSession());
        mLooper = std::make_unique<Looper>(*mSession);
        mThread = std::thread([this] { mLooper->run(); });
    }

    void TearDown() override {
        if (mLooper) {
            mLooper->stop();
        }
        if (mThread.joinable()) {
            mThread.join();
        }
        mLooper.reset();
        mSession.reset();
    }

    std::unique_ptr<Session> mSession;
    std::unique_ptr<Looper> mLooper;
    std::thread mThread;
};

TEST_F(LooperTest, PostRunsOnLoopThread) {
    std::thread::id loopTid;
    bool ownerInside = false;
    std::atomic<bool> done{false};

    mLooper->post([&] {
        loopTid = std::this_thread::get_id();
        ownerInside = mLooper->isOwnerThread();
        done = true;
    });

    ASSERT_TRUE(Dbusxx::UnitTest::waitUntil([&] { return done.load(); }));

    //! Task ran on the loop thread, not the posting (main) thread.
    EXPECT_NE(loopTid, std::this_thread::get_id());
    //! Inside the loop the owner-thread check is true; from main it is false.
    EXPECT_TRUE(ownerInside);
    EXPECT_FALSE(mLooper->isOwnerThread());
}

TEST_F(LooperTest, PostOrderPreserved) {
    std::vector<int> order;
    std::mutex mu;
    std::atomic<int> count{0};

    for (int i = 0; i < 5; ++i) {
        mLooper->post([&, i] {
            std::lock_guard<std::mutex> lk(mu);
            order.push_back(i);
            ++count;
        });
    }

    ASSERT_TRUE(Dbusxx::UnitTest::waitUntil([&] { return count.load() == 5; }));
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST_F(LooperTest, StopEndsRun) {
    //! Posting a stop makes run() return; the loop thread then joins cleanly.
    mLooper->post([this] { mLooper->stop(); });
    mThread.join();
    SUCCEED();  //! reached only if run() returned
}
