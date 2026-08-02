#ifndef SSDBUS_RAW_MESSAGE_SHARE_PTR_HPP
#define SSDBUS_RAW_MESSAGE_SHARE_PTR_HPP

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawRemoteError.hpp"

namespace SSDbus {
namespace Adaptor {
namespace RawMessage {
enum class Type : uint8_t {
    Invalid = _SD_BUS_MESSAGE_TYPE_INVALID,
    MethodCall = SD_BUS_MESSAGE_METHOD_CALL,
    MethodReturn = SD_BUS_MESSAGE_METHOD_RETURN,
    MethodError = SD_BUS_MESSAGE_METHOD_ERROR,
    Signal = SD_BUS_MESSAGE_SIGNAL,
    Max = _SD_BUS_MESSAGE_TYPE_MAX
};

RawBusMessagePtr createSignal(RawBusPtr aBus,
    std::string_view aPath, std::string_view aInterface, std::string_view aMember) {
    if (!aBus) {
        return nullptr;
    }

    if (!RawCheck::isPathNameValid(aPath)
        || !RawCheck::isInterfaceNameValid(aInterface)
        || !RawCheck::isMemberNameValid(aMember)) {
        return nullptr;
    }

    RawBusMessagePtr raw = nullptr;
    sd_bus_message_new_signal(
        aBus, &raw, aPath.data(), aInterface.data(), aMember.data());

    return raw;
}

RawBusMessagePtr createSignal(RawBusPtr aBus,
    const std::string& aPath, const std::string& aInterface, const std::string& aMember) {
    return createSignal(aBus, aPath, aInterface, aMember);
}

RawBusMessagePtr createMethodCall(RawBusPtr aBus,
    std::string_view aDestination, std::string_view aPath, std::string_view aInterface, std::string_view aMember) {
    if (!aBus) {
        return nullptr;
    }

    if (!RawCheck::isServiceNameValid(aDestination)
        || !RawCheck::isPathNameValid(aPath)
        || !RawCheck::isInterfaceNameValid(aInterface)
        || !RawCheck::isMemberNameValid(aMember)) {
        return nullptr;
    }

    RawBusMessagePtr raw = nullptr;
    sd_bus_message_new_method_call(aBus, &raw,
        aDestination.data(), aPath.data(), aInterface.data(), aMember.data());
    return raw;
}

RawBusMessagePtr createMethodCall(
    RawBusPtr aBus, const std::string& aDestination, const std::string& aPath, const std::string& aInterface, const std::string& aMember) {
    return createMethodCall(
        aBus, aDestination, aPath, aInterface, aMember);
}

RawBusMessagePtr createMethodReturn(RawBusMessagePtr aCall) {
    if (!aCall) {
        return nullptr;
    }

    RawBusMessagePtr raw = nullptr;
    sd_bus_message_new_method_return(aCall, &raw);
    return raw;
}

RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, const RawBusErrorPtr aErr) {
    if (!aCall || !aErr) {
        return nullptr;
    }

    RawBusMessagePtr raw = nullptr;
    sd_bus_message_new_method_error(aCall, &raw, aErr);
    return raw;
}

RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, int aErrno) {
    if (!aCall) {
        return nullptr;
    }

    RawBusMessagePtr raw = nullptr;
    sd_bus_message_new_method_errno(aCall, &raw, aErrno, nullptr);
    return raw;
}

void closeMessage(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return;
    }

    sd_bus_message_unref(aMsg);
}

Status sealMessage(RawBusMessagePtr aMsg, uint64_t aCookie, uint64_t aTimeout) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_message_seal(
        aMsg, aCookie, aTimeout));
}

Type getType(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return Type::Invalid;
    }

    uint8_t type;
    int ret = sd_bus_message_get_type(aMsg, &type);
    if (ret < 0) {
        return Type::Invalid;
    }

    static constexpr auto minimum = static_cast<uint8_t>(Type::Invalid);
    static constexpr auto maximum = static_cast<uint8_t>(Type::Max);
    if (type <= minimum || type >= maximum) {
        return Type::Invalid;
    }

    return static_cast<Type>(type);
}

std::string_view getSignature(RawBusMessagePtr aMsg, bool aComplete = true) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* sig = sd_bus_message_get_signature(aMsg, aComplete);
    return sig ? std::string_view(sig) : std::string_view();
}

std::string_view getPath(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* path = sd_bus_message_get_path(aMsg);
    return path ? std::string_view(path) : std::string_view();
}

std::string_view getInterface(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* interface = sd_bus_message_get_interface(aMsg);
    return interface ? std::string_view(interface) : std::string_view();
}

std::string_view getMember(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* member = sd_bus_message_get_member(aMsg);
    return member ? std::string_view(member) : std::string_view();
}

std::string_view getDestination(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* destination = sd_bus_message_get_destination(aMsg);
    return destination ? std::string_view(destination) : std::string_view();
}

std::string_view getSender(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string_view();
    }

    const char* sender = sd_bus_message_get_sender(aMsg);
    return sender ? std::string_view(sender) : std::string_view();
}

const RawBusError* getError(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return nullptr;
    }

    return sd_bus_message_get_error(aMsg);
}

RawBusPtr getBus(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return nullptr;
    }

    return sd_bus_message_get_bus(aMsg);
}

RawBusCredsPtr getCreds(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return nullptr;
    }

    return sd_bus_message_get_creds(aMsg);
}

bool isSignal(
    RawBusMessagePtr aMsg, std::string_view aInterface, std::string_view aMember) {
    if (!aMsg) {
        return false;
    }

    return sd_bus_message_is_signal(aMsg, aInterface.data(), aMember.data()) > 0;
}

bool isSignal(
    RawBusMessagePtr aMsg, const std::string& aInterface, const std::string& aMember) {
    if (!aMsg) {
        return false;
    }

    return isSignal(aMsg, aInterface, aMember);
}

bool isMethodCall(
    RawBusMessagePtr aMsg, std::string_view aInterface, std::string_view aMember) {
    if (!aMsg) {
        return false;
    }

    return sd_bus_message_is_method_call(aMsg, aInterface.data(), aMember.data()) > 0;
}

bool isMethodCall(
    RawBusMessagePtr aMsg, const std::string& aInterface, const std::string& aMember) {
    if (!aMsg) {
        return false;
    }

    return isMethodCall(aMsg, aInterface, aMember);
}

bool isMethodError(RawBusMessagePtr aMsg, std::string_view aName) {
    if (!aMsg) {
        return false;
    }

    return sd_bus_message_is_method_error(aMsg, aName.data()) > 0;
}

bool isMethodError(RawBusMessagePtr aMsg, const std::string& aName) {
    if (!aMsg) {
        return false;
    }

    return isMethodError(aMsg, aName);
}

bool isEmpty(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return false;
    }

    return sd_bus_message_is_empty(aMsg) > 0;
}

bool hasSignature(RawBusMessagePtr aMsg, std::string_view aSignature) {
    if (!aMsg) {
        return false;
    }

    return sd_bus_message_has_signature(aMsg, aSignature.data()) > 0;
}

bool hasSignature(RawBusMessagePtr aMsg, const std::string& aSignature) {
    return hasSignature(aMsg, aSignature);
}

Status setDestination(RawBusMessagePtr aMsg, std::string_view aDestination) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_message_set_destination(
        aMsg, aDestination.data()));
}

Status setDestination(RawBusMessagePtr aMsg, const std::string& aDestination) {
    return setDestination(aMsg, aDestination);
}

Status setSender(RawBusMessagePtr aMsg, std::string_view aSender) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_set_sender(aMsg, aSender.data()));
}

Status setSender(RawBusMessagePtr aMsg, const std::string& aSender) {
    return setSender(aMsg, aSender);
}

template<typename... Values>
Status append(RawBusMessagePtr aMsg, std::string_view aTypes, Values&&... aValues) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_append(aMsg, aTypes.data(), std::forward<Values>(aValues)...)
    );
}

template<typename... Values>
Status append(RawBusMessagePtr aMsg, const std::string& aTypes, Values&&... aValues) {
    return append(aMsg, aTypes, std::forward<Values>(aValues)...);
}

Status openContainer(RawBusMessagePtr aMsg, char aType, const char* aInType) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_open_container(aMsg, aType, aInType)
    );
}

Status closeContainer(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_close_container(aMsg)
    );
}

template<typename T>
Status appendBasic(RawBusMessagePtr aMsg, char aType, T&& aValue) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_append_basic(aMsg, aType, std::forward<T>(aValue))
    );
}

template<typename... Values>
Status pop(RawBusMessagePtr aMsg, std::string_view aTypes, Values&... aValues) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_read(aMsg, aTypes.data(), &aValues...)
    );
}

template<typename... Values>
Status pop(RawBusMessagePtr aMsg, const std::string& aTypes, Values&... aValues) {
    return pop(aMsg, aTypes.c_str(), aValues...);
}

Status enterContainer(RawBusMessagePtr aMsg, char aType, const char* aInType) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_enter_container(aMsg, aType, aInType)
    );
}

Status exitContainer(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_exit_container(aMsg)
    );
}

Status skip(RawBusMessagePtr aMsg, const char* aTypes) {
    return RawErrorConvert::makeStatus(
        sd_bus_message_skip(aMsg, aTypes)
    );
}

template<typename T>
Status popBasic(RawBusMessagePtr aMsg, char aType, T& aValue) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_read_basic(aMsg, aType, &aValue)
    );
}

std::string nextType(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return std::string();
    }

    char type = '\0';
    const char* containerType = nullptr;
    sd_bus_message_peek_type(aMsg, &type, &containerType);
    if (type == '\0') {
        return std::string();
    }

    std::string types;
    types.push_back(type);
    if (containerType) {
        types.append(containerType);
    }

    return types;
}

bool isEnd(RawBusMessagePtr aMsg, bool aComplete) {
    if (!aMsg) {
        return true;
    }

    int ret = sd_bus_message_at_end(aMsg, aComplete);
    if (ret < 0) {
        throw DbusException(
            "Failed to judge message end: ", strerror(-ret));
    }

    return ret > 0;
}

Status copyMessage(RawBusMessagePtr aSrc, RawBusMessagePtr& aDst, bool aIsAll = true) {
    if (!aSrc) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_message_copy(aSrc, aDst, aIsAll));
}

void unrefMessage(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return;
    }

    sd_bus_message_unref(aMsg);
}

void refMessage(RawBusMessagePtr aMsg) {
    if (!aMsg) {
        return;
    }

    sd_bus_message_ref(aMsg);
}

}

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
        if (mRaw && mIsOwned) {
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

        if (mRaw && mIsOwned) {
            RawMessage::unrefMessage(mRaw);
        }

        mRaw = aPtr.mRaw;
        mIsOwned = aPtr.mIsOwned;
        if (mRaw && mIsOwned) {
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
            aBus, aService, aPath, aIface, aMethod 
        );
        return RawMessageSharePtr(call, true);
    }

private:
    RawBusMessagePtr mRaw { nullptr };
    bool mIsOwned { false };
};

}
}


#endif