#ifndef SSDBUS_RAW_MESSAGE_SHARE_PTR_HPP
#define SSDBUS_RAW_MESSAGE_SHARE_PTR_HPP

#include "adaptor/RawAdaptor.hpp"

namespace SSDbus {
namespace Adaptor {

class RawMessageSharePtr {
public:
    explicit RawMessageSharePtr(RawBusMessagePtr aRawMsg, bool aIsOwned = false)
        : mRaw(aRawMsg), mIsOwned(aIsOwned) {}

    ~RawMessageSharePtr() {
        if (mIsOwned && mRaw) {
            RawMessage::unrefMessage(mRaw);
        }
        mRaw = nullptr;
    }

    RawMessageSharePtr(const RawMessageSharePtr& aPtr)
        : mRaw(aPtr.mRaw)
        , mIsOwned(aPtr.mIsOwned) {
        if (mRaw) {
            RawMessage::refMessage(mRaw);
        }
    }

    // 移动 → 转移所有权
    RawMessageSharePtr(RawMessageSharePtr&& aPtr) noexcept
        : mRaw(aPtr.mRaw)
        , mIsOwned(aPtr.mIsOwned) {
        aPtr.mRaw = nullptr;
        aPtr.mIsOwned = false;
    }

    RawMessageSharePtr& operator=(const RawMessageSharePtr& aPtr) {
        if (this == &aPtr) {
            return *this;
        }

        if (mIsOwned && mRaw) {
            RawMessage::unrefMessage(mRaw);
        }

        mRaw = aPtr.mRaw;
        mIsOwned = aPtr.mIsOwned;
        if (mRaw) {
            RawMessage::refMessage(mRaw);
        }

        return *this;
    }

    RawMessageSharePtr& operator=(RawMessageSharePtr&& aPtr) noexcept {
        if (this == &aPtr) {
            return *this;
        } 

        if (mIsOwned && mRaw) {
            RawMessage::unrefMessage(mRaw);
        }

        mRaw = aPtr.mRaw;
        mIsOwned = aPtr.mIsOwned;
        aPtr.mRaw = nullptr;
        aPtr.mIsOwned = false;
        return *this;
    }

    RawBusMessagePtr get() const {
        return mRaw;
    }

    explicit operator bool() const {
        return mRaw != nullptr;
    }

    static RawMessageSharePtr createReply(RawBusMessagePtr aCallMsg) {
        auto reply = Adaptor::RawMessage::createMethodReturn(aCallMsg);
        return RawMessageSharePtr(reply, true);
    }

    static RawMessageSharePtr createMethodCall(
        Adaptor::RawBusPtr aBus, std::string_view aService,
        std::string_view aPath, std::string_view aIface, std::string_view aMethod) {
        auto call = Adaptor::RawMessage::createMethodCall(
            aBus, aService.data(), aPath.data(), aIface.data(), aMethod.data() 
        );
        return RawMessageSharePtr(call, false);
    }

private:
    RawBusMessagePtr mRaw { nullptr };
    bool mIsOwned { false };
};

}
}


#endif