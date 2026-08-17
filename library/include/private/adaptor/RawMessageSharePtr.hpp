#ifndef DBUSXX_RAW_MESSAGE_SHARE_PTR_HPP
#define DBUSXX_RAW_MESSAGE_SHARE_PTR_HPP

#include "private/adaptor/RawCommon.hpp"
#include "private/adaptor/RawRemoteError.hpp"


namespace Dbusxx {
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

RawBusMessagePtr createSignal(RawBusPtr aBus, std::string_view aPath,
    std::string_view aInterface, std::string_view aMember);

RawBusMessagePtr createSignal(RawBusPtr aBus, const std::string& aPath,
    const std::string& aInterface, const std::string& aMember);

RawBusMessagePtr createMethodCall(RawBusPtr aBus, std::string_view aDestination,
    std::string_view aPath, std::string_view aInterface, std::string_view aMember);

RawBusMessagePtr createMethodCall(RawBusPtr aBus, const std::string& aDestination,
    const std::string& aPath, const std::string& aInterface, const std::string& aMember);

RawBusMessagePtr createMethodReturn(RawBusMessagePtr aCall);

RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, const RawBusErrorPtr aErr);

RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, int aErrno);

void refMessage(RawBusMessagePtr aMsg);

void unrefMessage(RawBusMessagePtr aMsg);

Status sealMessage(RawBusMessagePtr aMsg, uint64_t aCookie, uint64_t aTimeout);

Type getType(RawBusMessagePtr aMsg);

std::string getSignature(RawBusMessagePtr aMsg, bool aComplete = true);

std::string getPath(RawBusMessagePtr aMsg);

std::string getInterface(RawBusMessagePtr aMsg);

std::string getMember(RawBusMessagePtr aMsg);

std::string getDestination(RawBusMessagePtr aMsg);

std::string getSender(RawBusMessagePtr aMsg);

const RawBusError* getError(RawBusMessagePtr aMsg);

RawBusPtr getBus(RawBusMessagePtr aMsg);

RawBusCredsPtr getCreds(RawBusMessagePtr aMsg);

bool isSignal(RawBusMessagePtr aMsg,
    std::string_view aInterface, std::string_view aMember);

bool isSignal(RawBusMessagePtr aMsg,
    const std::string& aInterface, const std::string& aMember);

bool isMethodCall(RawBusMessagePtr aMsg,
    std::string_view aInterface, std::string_view aMember);

bool isMethodCall(RawBusMessagePtr aMsg,
    const std::string& aInterface, const std::string& aMember);

bool isMethodError(RawBusMessagePtr aMsg, std::string_view aName);

bool isMethodError(RawBusMessagePtr aMsg, const std::string& aName);

bool isEmpty(RawBusMessagePtr aMsg);

bool hasSignature(RawBusMessagePtr aMsg, std::string_view aSignature);

bool hasSignature(RawBusMessagePtr aMsg, const std::string& aSignature);

Status setDestination(RawBusMessagePtr aMsg, std::string_view aDestination);

Status setDestination(RawBusMessagePtr aMsg, const std::string& aDestination);

Status setSender(RawBusMessagePtr aMsg, std::string_view aSender);

Status setSender(RawBusMessagePtr aMsg, const std::string& aSender);

Status openContainer(RawBusMessagePtr aMsg, char aType, const char* aInType);

Status closeContainer(RawBusMessagePtr aMsg);

Status enterContainer(RawBusMessagePtr aMsg, char aType, const char* aInType);

Status exitContainer(RawBusMessagePtr aMsg);

Status skip(RawBusMessagePtr aMsg, const char* aTypes);

std::string nextType(RawBusMessagePtr aMsg);

bool isEnd(RawBusMessagePtr aMsg, bool aComplete);

Status copyMessage(RawBusMessagePtr aSrc, RawBusMessagePtr& aDst, bool aIsAll = true);

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

template<typename T>
Status popBasic(RawBusMessagePtr aMsg, char aType, T& aValue) {
    if (!aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_message_read_basic(aMsg, aType, &aValue)
    );
}
}

class RawMessageSharePtr {
public:
    explicit RawMessageSharePtr(RawBusMessagePtr aRawMsg, bool aIsOwned = false);

    ~RawMessageSharePtr();

    RawMessageSharePtr(const RawMessageSharePtr& aPtr);

    RawMessageSharePtr(RawMessageSharePtr&& aPtr) noexcept;

    RawMessageSharePtr& operator=(const RawMessageSharePtr& aPtr);

    RawMessageSharePtr& operator=(RawMessageSharePtr&& aPtr) noexcept;

    inline RawBusMessagePtr get() const {
        return mRaw;
    }

    explicit inline operator bool() const {
        return mRaw != nullptr;
    }

    static RawMessageSharePtr createReply(RawBusMessagePtr aCallMsg);

    static RawMessageSharePtr createMethodCall(
        Adaptor::RawBusPtr aBus, std::string_view aService,
        std::string_view aPath, std::string_view aIface, std::string_view aMethod);

private:
    RawBusMessagePtr mRaw { nullptr };
    bool mIsOwned { false };
};

}
}


#endif