#ifndef SSDBUS_RAW_SLOT_SHARE_PTR_HPP
#define SSDBUS_RAW_SLOT_SHARE_PTR_HPP

#include <systemd/sd-bus.h>

#include "adaptor/RawAdaptor.hpp"

namespace SSDbus {
namespace Adaptor {

class RawSlotSharePtr {
public:
    RawSlotSharePtr() = default;

    explicit RawSlotSharePtr(Adaptor::RawBusSlotPtr aRawSolt)
        : mRawSlot(aRawSolt) {}

    ~RawSlotSharePtr() {
        if (mRawSlot) {
            Adaptor::RawSlot::unrefSlot(mRawSlot);
        }
    }

    RawSlotSharePtr(const RawSlotSharePtr& aOther)
        : mRawSlot(aOther.mRawSlot) {
            if (aOther.mRawSlot) {
                Adaptor::RawSlot::refSlot(mRawSlot);
            }
    }

    RawSlotSharePtr(RawSlotSharePtr&& aOther) noexcept
        : mRawSlot(aOther.mRawSlot) {
        aOther.mRawSlot = nullptr;
    }

    RawSlotSharePtr& operator=(const RawSlotSharePtr& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (mRawSlot) {
            Adaptor::RawSlot::unrefSlot(mRawSlot);
        }

        if (aOther.mRawSlot) {
            mRawSlot = aOther.mRawSlot;
            Adaptor::RawSlot::refSlot(mRawSlot);
        }

        return *this;
    }

    RawSlotSharePtr& operator=(RawSlotSharePtr&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        if (mRawSlot) {
            Adaptor::RawSlot::unrefSlot(mRawSlot);
        }

        mRawSlot = aOther.mRawSlot;
        aOther.mRawSlot = nullptr;
        return *this;
    }

private:
    Adaptor::RawBusSlotPtr mRawSlot { nullptr };
};

}
}


#endif