
#ifndef SSDBUS_REPLY_ASYNC_HANDLER_HPP
#define SSDBUS_REPLY_ASYNC_HANDLER_HPP

#include <functional>
#include <memory>

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawSlotSharePtr.hpp"
#include "MessagePrivate.hpp"


namespace SSDbus {
namespace Private {
struct ReplyAsyncHandler : public MessagePrivate,
    public std::enable_shared_from_this<ReplyAsyncHandler> {
    using Callback = std::function<void(MessagePrivate*)>;

    static int onReply(Adaptor::RawBusMessagePtr aRep,
        void* aUsr, Adaptor::RawBusErrorPtr aErr);

    void setCallback(Callback aCallback);

    Adaptor::RawSlotSharePtr mSlot { nullptr };
    std::function<void(MessagePrivate*)> mCallback { nullptr };
    bool isFinished { false };
};

}
}
#endif