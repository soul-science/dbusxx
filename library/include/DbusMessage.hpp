#ifndef SSDBUS_DBUS_MESSAGE_HPP
#define SSDBUS_DBUS_MESSAGE_HPP

#include <systemd/sd-bus.h>

class DbusMessage {
public:
    DbusMessage() = default;

    explicit DbusMessage(sd_bus_message* aRawMsg)
        : mRawMsg(aRawMsg) {}

    ~DbusMessage() {
        if (mRawMsg) {
            sd_bus_message_unref(mRawMsg);
        }
    }

    DbusMessage(DbusMessage&& aOther) noexcept
        : mRawMsg(aOther.mRawMsg) {
            aOther.mRawMsg = nullptr;
    }

    DbusMessage& operator=(DbusMessage&& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (mRawMsg) {
            sd_bus_message_unref(mRawMsg);
        }

        mRawMsg = aOther.mRawMsg;
        aOther.mRawMsg = nullptr;
        return *this;
    }

    DbusMessage(const DbusMessage&) = delete;
    DbusMessage& operator=(const DbusMessage&) = delete;

private:
    sd_bus_message* mRawMsg { nullptr };
};


#endif