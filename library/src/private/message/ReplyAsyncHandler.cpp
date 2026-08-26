#include "private/message/ReplyAsyncHandler.hpp"

#include "private/adaptor/RawRemoteError.hpp"


namespace Dbusxx {
namespace Private {
int ReplyAsyncHandler::onReply(Adaptor::RawBusMessagePtr aRep,
    void* aUsr, Adaptor::RawBusErrorPtr aErr) {
    auto* self = static_cast<ReplyAsyncHandler*>(aUsr);
    if (!self) {
        Adaptor::RawRemoteError::fromStatus(StatusCode::UNKNOWN_ERROR).moveTo(aErr);
        return -1;
    }

    auto guard = self->shared_from_this();
    self->mRawMsg = Adaptor::RawMessageSharePtr(aRep);
    if (self->mCallback) {
        self->mCallback(self);
    }

    self->isFinished.store(true, std::memory_order_release);
    return 0;
}

void ReplyAsyncHandler::setCallback(Callback aCallback) {
    mCallback = aCallback;
    if (isFinished.load(std::memory_order_acquire)) {
        mCallback(this);
    }
}
}
}