#ifndef DBUSXX_RAW_EVENT_SHARE_PTR_HPP
#define DBUSXX_RAW_EVENT_SHARE_PTR_HPP

#include <cstdint>

#include "private/adaptor/RawCommon.hpp"
#include "private/adaptor/RawRemoteError.hpp"


namespace Dbusxx {
namespace Adaptor {
namespace RawEvent {
    Status createEvent(RawBusEventPtr& aRawEvent);

    void refEvent(RawBusEventPtr aRawEvent);

    void unrefEvent(RawBusEventPtr aRawEvent);

    Status addIO(RawBusEventPtr aRawEvent, RawBusEventSrcPtr& aEventSrc,
        int aFd, uint32_t aEvents, RawEventIOHandler aCallback, void* aData);

    Status loop(RawBusEventPtr aRawEvent);

    Status exit(RawBusEventPtr aRawEvent, int aCode);
}

namespace RawEventSrc {
    Status enableEventSrc(RawBusEventSrcPtr aSrc, int aType);

    void unrefEventSrc(RawBusEventSrcPtr aSrc);
}

class RawEventSharePtr {
public:
    explicit RawEventSharePtr(const RawBusEventPtr& aRawPtr, const Status& aStatus);

    ~RawEventSharePtr();

    RawEventSharePtr(const RawEventSharePtr& aOther);

    RawEventSharePtr& operator=(const RawEventSharePtr& aOther);

    RawEventSharePtr(RawEventSharePtr&& aOther) noexcept;

    RawEventSharePtr& operator=(RawEventSharePtr&& aOther) noexcept;

    static RawEventSharePtr make();

    inline RawBusEventPtr get() const {
        return mRawEvent;
    }

    inline Status status() const {
        return mStatus;
    }

private:
    RawBusEventPtr mRawEvent { nullptr };
    Status mStatus { StatusCode::UNKNOWN_ERROR };
};
}
}

#endif