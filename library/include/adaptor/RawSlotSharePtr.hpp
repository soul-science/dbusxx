#ifndef SSDBUS_RAW_SLOT_SHARE_PTR_HPP
#define SSDBUS_RAW_SLOT_SHARE_PTR_HPP

#include <systemd/sd-bus.h>

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawRemoteError.hpp"

namespace SSDbus {
namespace Adaptor {

namespace RawSlot {
void unrefSlot(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return;
    }

    sd_bus_slot_unref(aSlot);
}

void refSlot(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return;
    }

    sd_bus_slot_ref(aSlot);
}

RawBusPtr getBus(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return nullptr;
    }

    return sd_bus_slot_get_bus(aSlot);
}

void* getUserdata(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return nullptr;
    }

    return sd_bus_slot_get_userdata(aSlot);
}

void setUserdata(RawBusSlotPtr aSlot, void* aUserdata) {
    if (!aSlot) {
        return;
    }

    sd_bus_slot_set_userdata(aSlot, aUserdata);
}

//! TODO:
bool getSlotFloating(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return false;
    }

    int ret = sd_bus_slot_get_floating(aSlot);
    if (ret < 0) {
        throw DbusException(
            "Failed to get slot floating: ", strerror(-ret));
    }

    return ret > 0;
}

Status setSlotFloating(RawBusSlotPtr aSlot, bool isFloating) {
    if (!aSlot) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_slot_set_floating(aSlot, isFloating)
    );
}

Status setSlotDeleterCallback(RawBusSlotPtr aSlot, RawDeleterCallback aCallback) {
    if (!aSlot) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_slot_set_destroy_callback(aSlot, aCallback)
    );
}

RawDeleterCallback getSlotDeleterCallback(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return nullptr;
    }

    RawDeleterCallback callback = nullptr;
    sd_bus_slot_get_destroy_callback(aSlot, &callback);
    return callback;
}

RawBusMessagePtr getCurMessage(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return nullptr;
    }

    return sd_bus_slot_get_current_message(aSlot);
}

RawBusMessageHandler getCurMessageHandler(RawBusSlotPtr aSlot) {
    if (!aSlot) {
        return nullptr;
    }

    return sd_bus_slot_get_current_handler(aSlot);
}
}

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