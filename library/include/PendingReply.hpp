#ifndef DBUSXX_DBUS_PENDING_REPLY_HPP
#define DBUSXX_DBUS_PENDING_REPLY_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "private/message/MessagePrivate.hpp"
#include "private/message/ReplyAsyncHandler.hpp"
#include "Message.hpp"
#include "Reply.hpp"


namespace Dbusxx {
/**
 * @brief Handle for a single asynchronous remote call.
 *
 * `PendingReply<Ret>` keeps the state of one in-flight `callAsync`
 * invocation. Obtain the result either by installing a callback with
 * `setCallback()`, or by blocking on `wait()` and reading `reply()`.
 */
template<typename Ret>
class PendingReply {
    static_assert(isValidArg<Ret>(), "Unsupported value type");
public:
    //! @brief Construct an empty (invalid) pending reply.
    PendingReply() = default;

    /**
     * @brief Construct a pending reply from an async reply handler.
     *
     * The handler is consumed immediately: the future is set up and, if the
     * reply has already arrived, it is delivered synchronously.
     *
     * @param aHandler async reply handler
     */
    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler)
        , mCallback(
            std::make_shared<std::function<void(Reply<Ret>)>>()) {
        if (mHandler) {
            onGetReply();
        }
    }

    //! @brief Return true if the call failed.
    [[nodiscard]] inline bool isError() const {
        return mHandler && mHandler->getStatus().isError();
    }

    //! @brief Return the error description when the call failed.
    [[nodiscard]] inline std::string errorMessage() const {
        return mHandler ? mHandler->getStatus().message() : std::string();
    }

    //! @brief Return the current status of the pending call.
    [[nodiscard]] inline Status getStatus() const {
        return mHandler ? mHandler->getStatus() : Status(StatusCode::INVALID_ARG);
    }

    /**
     * @brief Install a callback to be invoked when the reply arrives.
     *
     * Replaces any previously installed callback; the latest one wins.
     *
     * If the reply has already been delivered synchronously (for example the
     * call failed while marshalling the arguments, or the reply arrived before
     * this callback was installed), the callback is invoked immediately with
     * that reply so the completion is never lost.
     *
     * @param aCallback completion callback
     */
    void setCallback(std::function<void(Reply<Ret>)> aCallback) {
        bool deliver = false;
        std::function<void(Reply<Ret>)> cb;
        {
            std::lock_guard lock(*mMutex);
            if (!mCallback) {
                mCallback = std::make_shared<std::function<void(Reply<Ret>)>>();
            }

            *mCallback = std::move(aCallback);
            cb = *mCallback;
            if (mHandler && mHandler->isFinished.load()) {
                mReply = mFuture.get();
                deliver = true;
            }
        }

        if (deliver) {
            cb(mReply);
        }
    }

    //! @brief Block until the reply arrives.
    inline void wait() {
        mReply = mFuture.get();
    }

    /**
     * @brief Wait for the reply with a timeout.
     *
     * Blocks up to `aTimeoutMs` milliseconds for the reply to arrive.
     * Pass a concrete millisecond value for a bounded wait; `0` performs a
     * non-blocking check and returns immediately with the current state.
     * Use `wait()` for an unbounded (infinite) wait.
     *
     * @param aTimeoutMs timeout in milliseconds (0 = non-blocking check)
     * @return true if the reply arrived within the timeout; false on timeout.
     *         Only read `reply()` after a `true` return.
    */
    [[nodiscard]] bool waitFor(std::size_t aTimeoutMs) {
        if (mFuture.wait_for(std::chrono::milliseconds(aTimeoutMs))
                == std::future_status::timeout) {
            return false;
        }

        mReply = mFuture.get();
        return true;
    }

    //! @brief Return the reply obtained by `wait()`.
    [[nodiscard]] inline Reply<Ret> reply() const {
        return mReply;
    }

private:
    void onGetReply() {
        auto promisePtr = std::make_shared<std::promise<Reply<Ret>>>();
        mFuture = promisePtr->get_future().share();
        mHandler->setCallback(
            [mCallback = mCallback,
             mMutex = mMutex, promisePtr] (Private::MessagePrivate* aPrivate) {
                Reply<Ret> rep(std::make_shared<Private::MessagePrivate>(*aPrivate));
                promisePtr->set_value(rep);
                std::function<void(Reply<Ret>)> cb;
                {
                    std::lock_guard lock(*mMutex);
                    cb = *mCallback;
                }

                if (cb) {
                    cb(rep);
                }
        });
    }

    std::shared_ptr<Private::ReplyAsyncHandler> mHandler { nullptr };
    std::shared_ptr<std::mutex> mMutex { std::make_shared<std::mutex>() };
    std::shared_ptr<std::function<void(Reply<Ret>)>> mCallback { nullptr };
    std::shared_future<Reply<Ret>> mFuture {};
    Reply<Ret> mReply{};
};

//! @brief Specialization for void-returning asynchronous calls.
template<>
class PendingReply<void> {
public:
    //! @brief Construct an empty (invalid) pending reply.
    PendingReply() = default;

    /**
     * @brief Construct a pending reply from an async reply handler.
     *
     * The handler is consumed immediately: the future is set up and, if the
     * reply has already arrived, it is delivered synchronously.
     *
     * @param aHandler async reply handler
     */
    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler)
        , mCallback(
            std::make_shared<std::function<void(Reply<void>)>>()) {
        if (mHandler) {
            onGetReply();
        }
    }

    //! @brief Return true if the call failed.
    [[nodiscard]] bool isError() const {
        return mHandler && mHandler->getStatus().isError();
    }

    //! @brief Return the error description when the call failed.
    [[nodiscard]] std::string errorMessage() const {
        return mHandler ? mHandler->getStatus().message() : std::string();
    }

    //! @brief Return the current status of the pending call.
    [[nodiscard]] Status getStatus() const {
        return mHandler ? mHandler->getStatus() : Status(StatusCode::INVALID_ARG);
    }

    /**
     * @brief Install a callback to be invoked when the reply arrives.
     *
     * Replaces any previously installed callback; the latest one wins.
     *
     * If the reply has already been delivered synchronously (for example the
     * call failed while marshalling the arguments, or the reply arrived before
     * this callback was installed), the callback is invoked immediately with
     * that reply so the completion is never lost.
     *
     * @param aCallback completion callback
     */
    void setCallback(std::function<void(Reply<void>)> aCallback) {
        bool deliver = false;
        std::function<void(Reply<void>)> cb;
        {
            std::lock_guard lock(*mMutex);
            if (!mCallback) {
                mCallback = std::make_shared<std::function<void(Reply<void>)>>();
            }

            *mCallback = std::move(aCallback);
            cb = *mCallback;
            if (mHandler && mHandler->isFinished.load()) {
                mReply = mFuture.get();
                deliver = true;
            }
        }

        if (deliver) {
            cb(mReply);
        }
    }

    //! @brief Block until the reply arrives.
    inline void wait() {
        mReply = mFuture.get();
    }

    /**
     * @brief Wait for the reply with a timeout.
     *
     * Blocks up to `aTimeoutMs` milliseconds for the reply to arrive.
     * Pass a concrete millisecond value for a bounded wait; `0` performs a
     * non-blocking check and returns immediately with the current state.
     * Use `wait()` for an unbounded (infinite) wait.
     *
     * @param aTimeoutMs timeout in milliseconds (0 = non-blocking check)
     * @return true if the reply arrived within the timeout; false on timeout.
     *         Only read `reply()` after a `true` return.
    */
    [[nodiscard]] bool waitFor(std::size_t aTimeoutMs) {
        if (mFuture.wait_for(std::chrono::milliseconds(aTimeoutMs))
                == std::future_status::timeout) {
            return false;
        }

        mReply = mFuture.get();
        return true;
    }

    //! @brief Return the reply obtained by `wait()`.
    [[nodiscard]] Reply<void> reply() const {
        return mReply;
    }

private:
    void onGetReply() {
        auto promisePtr = std::make_shared<std::promise<Reply<void>>>();
        mFuture = promisePtr->get_future().share();
        mHandler->setCallback(
            [mCallback = mCallback,
             mMutex = mMutex, promisePtr] (Private::MessagePrivate* aPrivate) {
                Reply<void> rep(std::make_shared<Private::MessagePrivate>(*aPrivate));
                promisePtr->set_value(rep);
                std::function<void(Reply<void>)> cb;
                {
                    std::lock_guard lock(*mMutex);
                    cb = *mCallback;
                }
                if (cb) {
                    cb(rep);
                }
        });
    }

    std::shared_ptr<Private::ReplyAsyncHandler> mHandler { nullptr };
    std::shared_ptr<std::mutex> mMutex { std::make_shared<std::mutex>() };
    std::shared_ptr<std::function<void(Reply<void>)>> mCallback { nullptr };
    std::shared_future<Reply<void>> mFuture {};
    Reply<void> mReply {};
};

}

#endif