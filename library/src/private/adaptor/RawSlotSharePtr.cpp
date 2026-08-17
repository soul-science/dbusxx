#include "private/adaptor/RawSlotSharePtr.hpp"


namespace Dbusxx {
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
}

RawSlotSharePtr::RawSlotSharePtr(Adaptor::RawBusSlotPtr aRawSolt)
    : mRawSlot(aRawSolt) {}

RawSlotSharePtr::~RawSlotSharePtr() {
    if (mRawSlot) {
        Adaptor::RawSlot::unrefSlot(mRawSlot);
    }
}

RawSlotSharePtr::RawSlotSharePtr(const RawSlotSharePtr& aOther)
    : mRawSlot(aOther.mRawSlot) {
    if (aOther.mRawSlot) {
        Adaptor::RawSlot::refSlot(mRawSlot);
    }
}

RawSlotSharePtr::RawSlotSharePtr(RawSlotSharePtr&& aOther) noexcept
    : mRawSlot(aOther.mRawSlot) {
    aOther.mRawSlot = nullptr;
}

RawSlotSharePtr& RawSlotSharePtr::operator=(const RawSlotSharePtr& aOther) {
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

RawSlotSharePtr& RawSlotSharePtr::operator=(RawSlotSharePtr&& aOther) noexcept {
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
}
}