#ifndef SSDBUS_DBUS_PENDING_REPLY_HPP
#define SSDBUS_DBUS_PENDING_REPLY_HPP

#include <functional>
#include <memory>

#include "Message.hpp"
#include "Reply.hpp"

#include "adaptor/RawCommon.hpp"
#include "message/MessagePrivate.hpp"
#include "message/ReplyAsyncHandler.hpp"

namespace SSDbus {

template<typename Ret>
class PendingReply {
    static_assert(isValidArgs<Ret>(), "Unsupported value type");
public:
    PendingReply() = default;

    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler)
        : mHandler(aHandler) {}

    void setCallback(std::function<void(Reply<Ret>)> aCallback) {
        auto repPtr = std::make_shared<Reply<Ret>>();
        mHandler->setCallback(
            [repPtr, aCallback] (Private::MessagePrivate* aPrivate) {
                *repPtr = Reply<Ret>(std::make_shared<Private::MessagePrivate>(*aPrivate));
                aCallback(*repPtr);
        });
        mRepPtr = repPtr;
    }

    [[nodiscard]] bool isError() const {
        return mHandler->getStatus().isError();
    }

    [[nodiscard]] std::string errorMessage() const {
        return mHandler->getStatus().message();
    }

    [[nodiscard]] Status getStatus() const {
        return mHandler->getStatus();
    }

    [[nodiscard]] Reply<Ret> reply() const {
        return *mRepPtr;
    }

private:
    std::shared_ptr<Private::ReplyAsyncHandler> mHandler { nullptr };
    std::shared_ptr<Reply<Ret>> mRepPtr { std::make_shared<Reply<Ret>>() };
};

}

#endif