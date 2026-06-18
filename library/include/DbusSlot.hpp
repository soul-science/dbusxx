#ifndef SSDBUS_DBUS_SLOT_HPP
#define SSDBUS_DBUS_SLOT_HPP

#include <systemd/sd-bus.h>

class Slot {
public:
    Slot() = default;

    explicit Slot(sd_bus_slot* aRawSolt)
        : mRawSlot(aRawSolt) {}

    ~Slot() {
        if (mRawSlot) {
            sd_bus_slot_unref(mRawSlot);
        }
    }

    Slot(Slot&& aOther) noexcept
        : mRawSlot(aOther.mRawSlot) {
        aOther.mRawSlot = nullptr;
    }

    Slot& operator=(Slot&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        if (mRawSlot) {
            sd_bus_slot_unref(mRawSlot);
        }

        mRawSlot = aOther.mRawSlot;
        aOther.mRawSlot = nullptr;
        return *this;
    }

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

private:
    sd_bus_slot* mRawSlot { nullptr };
};


#endif