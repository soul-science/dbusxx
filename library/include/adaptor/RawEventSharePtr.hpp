#ifndef SSDBUS_RAW_EVENT_SHARE_PTR_HPP
#define SSDBUS_RAW_EVENT_SHARE_PTR_HPP

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawRemoteError.hpp"

namespace SSDbus {
namespace Adaptor {

namespace RawEvent {
    Status createEvent(RawBusEventPtr& aRawEvent) {
        return RawErrorConvert::makeStatus(sd_event_new(&aRawEvent));
    }

    void refEvent(RawBusEventPtr aRawEvent) {
        if (!aRawEvent) {
            return;
        }

        sd_event_ref(aRawEvent);
    }

    void unrefEvent(RawBusEventPtr aRawEvent) {
        if (!aRawEvent) {
            return;
        }

        sd_event_unref(aRawEvent);
    }

    Status addIO(RawBusEventPtr aRawEvent, RawBusEventSrcPtr& aEventSrc,
        int aFd, uint32_t aEvents, RawEventIOHandler aCallback, void* aData) {
        if (!aRawEvent) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawErrorConvert::makeStatus(
            sd_event_add_io(aRawEvent, &aEventSrc, aFd, aEvents, aCallback, aData));
    }

    Status loop(RawBusEventPtr aRawEvent) {
        if (!aRawEvent) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawErrorConvert::makeStatus(sd_event_loop(aRawEvent));
    }

    Status exit(RawBusEventPtr aRawEvent, int aCode) {
        if (!aRawEvent) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawErrorConvert::makeStatus(sd_event_exit(aRawEvent, aCode));
    }
}

class RawEventSharePtr {
public:
    RawEventSharePtr() {}

    explicit RawEventSharePtr(const RawBusEventPtr& aRawPtr, const Status& aStatus)
        : mRawEvent(aRawPtr)
        , mStatus(aStatus) {}

    ~RawEventSharePtr() {
        if (mRawEvent) {
            RawEvent::unrefEvent(mRawEvent);
        }

        mRawEvent = nullptr;
    }

    RawEventSharePtr(const RawEventSharePtr& aOther)
        : mRawEvent(aOther.mRawEvent)
        , mStatus(aOther.mStatus) {
            if (mRawEvent) {
                RawEvent::refEvent(mRawEvent);
            }
        }

    RawEventSharePtr& operator=(const RawEventSharePtr& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (aOther.mRawEvent) {
            mRawEvent = aOther.mRawEvent;
            mStatus = aOther.mStatus;
            RawEvent::refEvent(mRawEvent);
        }

        return *this;
    }

    RawEventSharePtr(RawEventSharePtr&& aOther) noexcept {
        if (mRawEvent) {
            RawEvent::unrefEvent(mRawEvent);
            mRawEvent = nullptr;
        }

        if (aOther.mRawEvent) {
            mRawEvent = std::move(aOther.mRawEvent);
            mStatus = std::move(aOther.mStatus);
            aOther.mRawEvent = nullptr;
            aOther.mStatus = Status(StatusCode::UNKNOWN_ERROR);
        }
    }

    RawEventSharePtr& operator=(RawEventSharePtr&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        if (mRawEvent) {
            RawEvent::unrefEvent(mRawEvent);
            mRawEvent = nullptr;
        }

        if (aOther.mRawEvent) {
            mRawEvent = std::move(aOther.mRawEvent);
            mStatus = std::move(aOther.mStatus);
            aOther.mRawEvent = nullptr;
            aOther.mStatus = Status(StatusCode::UNKNOWN_ERROR);
        }

        return *this;
    }

    static RawEventSharePtr make() {
        RawBusEventPtr raw = nullptr;
        Status st = RawEvent::createEvent(raw);
        return RawEventSharePtr(raw, st);
    }

    RawBusEventPtr get() const {
        return mRawEvent;
    }

    Status status() const {
        return mStatus;
    }

private:
    RawBusEventPtr mRawEvent { nullptr };
    Status mStatus { StatusCode::UNKNOWN_ERROR };
};
}
}

#endif