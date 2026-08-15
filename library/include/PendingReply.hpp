#ifndef SSDBUS_DBUS_PENDING_REPLY_HPP
#define SSDBUS_DBUS_PENDING_REPLY_HPP

#include <functional>
#include <future>
#include <memory>

#include "message/MessagePrivate.hpp"
#include "message/ReplyAsyncHandler.hpp"
#include "Message.hpp"
#include "Reply.hpp"

namespace SSDbus {
template<typename Ret>
class PendingReply {
    static_assert(isValidArgs<Ret>(), "Unsupported value type");
public:
    PendingReply() = default;

    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler) {}

    void setCallback(std::function<void(Reply<Ret>)> aCallback) {
        auto promisePtr = std::make_shared<std::promise<Reply<Ret>>>();
        mFuture = promisePtr->get_future().share();
        mHandler->setCallback(
            [promisePtr, aCallback] (Private::MessagePrivate* aPrivate) {
                Reply<Ret> rep(std::make_shared<Private::MessagePrivate>(*aPrivate));
                aCallback(rep);
                promisePtr->set_value(rep);
        });
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

    void wait() {
        mReply = mFuture.get();
    }

    [[nodiscard]] inline Reply<Ret> reply() const {
        return mReply;
    }

private:
    std::shared_ptr<Private::ReplyAsyncHandler> mHandler { nullptr };;
    std::shared_future<Reply<Ret>> mFuture{};
    Reply<Ret> mReply{};
};

template<>
class PendingReply<void> {
public:
    PendingReply() = default;

    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler) {}

    void setCallback(std::function<void(Reply<void>)> aCallback) {
        auto promisePtr = std::make_shared<std::promise<Reply<void>>>();
        mFuture = promisePtr->get_future().share();
        mHandler->setCallback(
            [promisePtr, aCallback] (Private::MessagePrivate* aPrivate) {
                Reply<void> rep(std::make_shared<Private::MessagePrivate>(*aPrivate));
                aCallback(rep);
                promisePtr->set_value(rep);
            });
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

    void wait() {
        mReply = mFuture.get();
    }

    [[nodiscard]] Reply<void> reply() const {
        return mReply;
    }

private:
    std::shared_ptr<Private::ReplyAsyncHandler> mHandler { nullptr };
    std::shared_future<Reply<void>> mFuture {};
    Reply<void> mReply {};
};

}

#endif