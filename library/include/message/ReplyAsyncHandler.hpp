
#ifndef SSDBUS_REPLY_ASYNC_HANDLER_HPP
#define SSDBUS_REPLY_ASYNC_HANDLER_HPP

#include <memory>

#include "adaptor/RawAdaptor.hpp"
#include "MessagePrivate.hpp"

namespace SSDbus {
namespace Private {

struct ReplyAsyncHandler : public MessagePrivate,
    public std::enable_shared_from_this<ReplyAsyncHandler> {
    using Callback = std::function<void(MessagePrivate*)>;

    static int onReply(Adaptor::RawBusMessagePtr aRep, void* aUsr, Adaptor::RawBusErrorPtr) {
        auto* self = static_cast<ReplyAsyncHandler*>(aUsr);
        auto guard = self->shared_from_this();
        self->mRawMsg = Adaptor::RawMessageSharePtr(aRep);
        if (self->mCallback) {
            self->mCallback(self);
        }

        self->isFinished = true;
        return 0;
    }

    void setCallback(Callback aCallback) {
        mCallback = aCallback;
        if (isFinished) {
            mCallback(this);
        }
    }

    std::function<void(MessagePrivate*)> mCallback { nullptr };
    bool isFinished { false };
};

}
}
#endif