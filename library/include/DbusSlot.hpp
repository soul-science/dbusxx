#ifndef SSDBUS_DBUS_SLOT_HPP
#define SSDBUS_DBUS_SLOT_HPP

#include <systemd/sd-bus.h>

class DbusSlot {
public:
    DbusSlot() = default;

    explicit DbusSlot(sd_bus_slot* aRawSolt)
        : mRawSlot(aRawSolt) {}

    ~DbusSlot() {
        if (mRawSlot) {
            sd_bus_slot_unref(mRawSlot);
        }
    }

    DbusSlot(DbusSlot&& aOther) noexcept
        : mRawSlot(aOther.mRawSlot) {
        aOther.mRawSlot = nullptr;
    }

    DbusSlot& operator=(DbusSlot&& aOther) noexcept {
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

    DbusSlot(const DbusSlot&) = delete;
    DbusSlot& operator=(const DbusSlot&) = delete;

private:
    sd_bus_slot* mRawSlot { nullptr };
};


#endif