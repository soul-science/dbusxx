#ifndef SSDBUS_DBUS_PENDING_REPLY_HPP
#define SSDBUS_DBUS_PENDING_REPLY_HPP

#include <functional>
#include <future>
#include <memory>
#include <mutex>

#include "private/message/MessagePrivate.hpp"
#include "private/message/ReplyAsyncHandler.hpp"
#include "Message.hpp"
#include "Reply.hpp"


namespace SSDbus {
template<typename Ret>
class PendingReply {
    static_assert(isValidArgs<Ret>(), "Unsupported value type");
public:
    PendingReply() = default;

    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler)
        , mCallback(
            std::make_shared<std::function<void(Reply<Ret>)>>()) {
        if (mHandler) {
            onGetPeply();
        }
    }

    [[nodiscard]] inline bool isError() const {
        return mHandler && mHandler->getStatus().isError();
    }

    [[nodiscard]] inline std::string errorMessage() const {
        return mHandler ? mHandler->getStatus().message() : std::string();
    }

    [[nodiscard]] inline Status getStatus() const {
        return mHandler->getStatus();
    }

    void setCallback(std::function<void(Reply<Ret>)> aCallback) {
        std::lock_guard lock(*mMutex);
        if (!mCallback) {
            mCallback = std::make_shared<std::function<void(Reply<Ret>)>>();
        }

        *mCallback = std::move(aCallback);
    }

    void wait() {
        mReply = mFuture.get();
    }

    [[nodiscard]] inline Reply<Ret> reply() const {
        return mReply;
    }

private:
    void onGetPeply() {
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

template<>
class PendingReply<void> {
public:
    PendingReply() = default;

    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler)
        , mCallback(
            std::make_shared<std::function<void(Reply<void>)>>()) {
        if (mHandler) {
            onGetPeply();
        }
    }

    [[nodiscard]] bool isError() const {
        return mHandler && mHandler->getStatus().isError();
    }

    [[nodiscard]] std::string errorMessage() const {
        return mHandler ? mHandler->getStatus().message() : std::string();
    }

    [[nodiscard]] Status getStatus() const {
        return mHandler ? mHandler->getStatus() : Status(StatusCode::INVALID_ARG);
    }

    void setCallback(std::function<void(Reply<void>)> aCallback) {
        std::lock_guard lock(*mMutex);
        if (!mCallback) {
            mCallback = std::make_shared<std::function<void(Reply<void>)>>();
        }

        *mCallback = std::move(aCallback);
    }

    void wait() {
        mReply = mFuture.get();
    }

    [[nodiscard]] Reply<void> reply() const {
        return mReply;
    }

private:
    void onGetPeply() {
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