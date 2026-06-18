#ifndef SSDBUS_RAW_BUS_SHARE_PTR_HPP
#define SSDBUS_RAW_BUS_SHARE_PTR_HPP

#include "adaptor/RawAdaptor.hpp"

namespace SSDbus {
namespace Adaptor {

class RawBusSharePtr {
public:
    explicit RawBusSharePtr(RawBusPtr aRawBusPtr, bool aIsOwned = false, bool aIsSystem = false)
        : mRaw(aRawBusPtr)
        , mIsOwned(aIsOwned)
        , mIsSystem(aIsSystem) {}

    ~RawBusSharePtr() {
        if (mIsOwned && mRaw) {
            RawBus::unrefBus(mRaw);
        }
        mRaw = nullptr;
    }

    RawBusSharePtr(const RawBusSharePtr& aPtr)
        : mRaw(aPtr.mRaw)
        , mIsOwned(aPtr.mIsOwned)
        , mIsSystem(aPtr.mIsSystem) {
        if (mRaw) {
            RawBus::refBus(mRaw);
        }
    }

    RawBusSharePtr(RawBusSharePtr&& aPtr) noexcept
    : mRaw(aPtr.mRaw)
    , mIsOwned(aPtr.mIsOwned)
    , mIsSystem(aPtr.mIsSystem) {
        aPtr.mRaw = nullptr;
        aPtr.mIsOwned = false;
        aPtr.mIsSystem = false;
    }

    RawBusSharePtr& operator=(const RawBusSharePtr& aPtr) {
        if (this == &aPtr) {
            return *this;
        }

        if (mIsOwned && mRaw) {
            RawBus::unrefBus(mRaw);
        }

        mRaw = aPtr.mRaw;
        mIsOwned = aPtr.mIsOwned;
        mIsSystem = aPtr.mIsSystem;
        if (mIsOwned && mRaw) {
            RawBus::refBus(mRaw);
        }

        return *this;
    }

    RawBusSharePtr& operator=(RawBusSharePtr&& aPtr) noexcept {
        if (this == &aPtr) {
            return *this;
        }

        if (mIsOwned && mRaw) {
            RawBus::unrefBus(mRaw);
        }

        mRaw = aPtr.mRaw;
        mIsOwned = aPtr.mIsOwned;
        mIsSystem = aPtr.mIsSystem;
        aPtr.mRaw = nullptr;
        aPtr.mIsOwned = false;
        aPtr.mIsSystem = false;

        return *this;
    }

    explicit operator bool() const {
        return mRaw != nullptr;
    }

    RawBusPtr get() const {
        return mRaw;
    }

    bool isOwned() const {
        return mIsOwned;
    }

    bool isSystem() const {
        return mIsSystem;
    }

    static RawBusSharePtr make(bool aIsSystem) {
        RawBusPtr raw = nullptr;
        int ret = aIsSystem ? sd_bus_open_system(&raw)
            : sd_bus_open_user(&raw);
        if (ret < 0) {
            throw DbusException("Failed to open bus: ", strerror(-ret));
        }

        return RawBusSharePtr(raw, true, aIsSystem);
    }

private:
    RawBusPtr mRaw { nullptr };
    bool mIsOwned { false };
    bool mIsSystem { false };
};

}
}
#endif