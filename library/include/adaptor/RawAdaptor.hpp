#ifndef SSDBUS_RAW_ADAPTOR_HPP
#define SSDBUS_RAW_ADAPTOR_HPP

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus-protocol.h>

#include <cassert>
#include <cerrno>
#include <string>
#include <string_view>
#include <stdexcept>

#include "Status.hpp"

#include <iostream>

namespace SSDbus {
namespace Adaptor {
using RawBus_ = sd_bus;
using RawBusPtr = sd_bus*;
using RawbusMessage_ = sd_bus_message;
using RawBusMessagePtr = sd_bus_message*;
using RawBusSlotPtr = sd_bus_slot*;
using RawBusCredsPtr = sd_bus_creds*;
using RawBusTrackPtr = sd_bus_track*;
using RawBusError = sd_bus_error;
using RawBusErrorPtr = sd_bus_error*;
using RawBusErrorMap = sd_bus_error_map;
using RawBusEventPtr = sd_event*;
using RawBusEventSrcPtr = sd_event_source*;
using RawBusVTable = sd_bus_vtable;

using RawBusMessageHandler = sd_bus_message_handler_t;
using RawBusPropertyGetter = sd_bus_property_get_t;
using RawBusPropertySetter = sd_bus_property_set_t;
using RawBusObjectFinder = sd_bus_object_find_t;
using RawBusNodeEnumerator = sd_bus_node_enumerator_t;
using RawBusTrackHandler = sd_bus_track_handler_t;
using RawDeleterCallback = sd_bus_destroy_t;

class DbusException : public std::runtime_error {
public:
    explicit DbusException(const std::string& aMsg)
        : std::runtime_error(aMsg) {}

    explicit DbusException(const std::string& aName, const std::string& msg)
        : std::runtime_error(aName + ": " + msg) {}
};

namespace RawError {
inline StatusCode fromErrno(int aErrno) {
    switch (aErrno) {
        case 0:
            return StatusCode::SUCCESS;
        case EINVAL:
            return StatusCode::INVALID_ARG;
        case ENOENT:
            return StatusCode::NOT_FOUND;
        case EHOSTUNREACH:
            return StatusCode::NO_SERVICE;
        case EBADR:
            return StatusCode::NO_METHOD;
        case EACCES:
            return StatusCode::ACCESS_DENIED;
        case EADDRINUSE:
        case EEXIST:
            return StatusCode::NAME_EXISTS;
        case ENOTCONN:
            return StatusCode::NOT_CONNECTED;
        case ECONNRESET:
            return StatusCode::CONN_RESET;
        case EBUSY:
            return StatusCode::BUSY;
        case ETIMEDOUT:
            return StatusCode::TIMEOUT;
        case ENOMEM:
            return StatusCode::NO_MEMORY;
        case ENOMSG:
            return StatusCode::NO_REPLY;
        case EBADMSG:
            return StatusCode::PROTOCOL_ERROR;
        case EIO:
            return StatusCode::IO_ERROR;
        case EMSGSIZE:
            return StatusCode::MSG_TOO_LONG;
        case E2BIG:
            return StatusCode::LIMIT_EXCEEDED;
        case EPROTO:
            return StatusCode::PROTOCOL_ERROR;
        case ENOTSUP:
            return StatusCode::PROTOCOL_ERROR;
        case ENXIO:
            return StatusCode::TYPE_MISMATCH;
        default:
            return StatusCode::UNKNOWN_ERROR;
    }
}

inline Status makeStatus(int aRet) {
    if (aRet >= 0) {
        return Status(StatusCode::SUCCESS);
    }

    std::cerr << "[DEBUG] makeStatus ret=" << aRet << " errno=" << (-aRet) << std::endl;

    return Status(RawError::fromErrno(-aRet));
}

}

namespace RawCheck {

    bool isInterfaceNameValid(std::string_view aName) {
        return sd_bus_interface_name_is_valid(aName.data()) > 0;
    }

    bool isInterfaceNameValid(const std::string& aName) {
        return isInterfaceNameValid(aName);
    }

    bool isServiceNameValid(std::string_view aName) {
        return sd_bus_service_name_is_valid(aName.data()) > 0;
    }

    bool isServiceNameValid(const std::string& aName) {
        return isServiceNameValid(aName);
    }

    bool isMemberNameValid(std::string_view aName) {
        return sd_bus_member_name_is_valid(aName.data()) > 0;
    }

    bool isMemberNameValid(const std::string& aName) {
        return isMemberNameValid(aName);
    }

    bool isPathNameValid(std::string_view aName) {
        return sd_bus_object_path_is_valid(aName.data()) > 0;
    }

    bool isPathNameValid(const std::string& aName) {
        return isPathNameValid(aName);
    }
}

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

        return RawError::makeStatus(
            sd_bus_slot_set_floating(aSlot, isFloating)
        );
    }

    Status setSlotDeleterCallback(RawBusSlotPtr aSlot, RawDeleterCallback aCallback) {
        if (!aSlot) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
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

        return RawError::makeStatus(sd_bus_message_seal(
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

        return RawError::makeStatus(sd_bus_message_set_destination(
            aMsg, aDestination.data()));
    }

    Status setDestination(RawBusMessagePtr aMsg, const std::string& aDestination) {
        return setDestination(aMsg, aDestination);
    }

    Status setSender(RawBusMessagePtr aMsg, std::string_view aSender) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
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

        return RawError::makeStatus(
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

        return RawError::makeStatus(
            sd_bus_message_open_container(aMsg, aType, aInType)
        );
    }

    Status closeContainer(RawBusMessagePtr aMsg) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
            sd_bus_message_close_container(aMsg)
        );
    }

    template<typename T>
    Status appendBasic(RawBusMessagePtr aMsg, char aType, T&& aValue) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
            sd_bus_message_append_basic(aMsg, aType, std::forward<T>(aValue))
        );
    }

    template<typename... Values>
    Status pop(RawBusMessagePtr aMsg, std::string_view aTypes, Values&... aValues) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }
 
        return RawError::makeStatus(
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

        return RawError::makeStatus(
            sd_bus_message_enter_container(aMsg, aType, aInType)
        );
    }

    Status exitContainer(RawBusMessagePtr aMsg) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
            sd_bus_message_exit_container(aMsg)
        );
    }

    template<typename T>
    Status popBasic(RawBusMessagePtr aMsg, char aType, T& aValue) {
        if (!aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
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

        return RawError::makeStatus(sd_bus_message_copy(aSrc, aDst, aIsAll));
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

namespace RawBus {
    Status openBus(RawBusPtr& aBus) {
        return RawError::makeStatus(sd_bus_open(&aBus));
    }

    Status openSystemBus(RawBusPtr& aBus) {
        return RawError::makeStatus(sd_bus_open_system(&aBus));
    }

    Status openUserBus(RawBusPtr& aBus) {
        return RawError::makeStatus(sd_bus_open_user(&aBus));
    }

    bool isBusReady(RawBusPtr aBus) {
        if (!aBus) {
            return false;
        }

        return sd_bus_is_ready(aBus) > 0;
    }

    bool isBusOpen(RawBusPtr aBus) {
        if (!aBus) {
            return false;
        }

        return sd_bus_is_open(aBus) > 0;
    }

    Status flushBus(RawBusPtr aBus) {
        if (!aBus) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_flush(aBus));
    }

    void unrefBus(RawBusPtr aBus) {
        if (!aBus) {
            return;
        }

        sd_bus_unref(aBus);
    }

    void refBus(RawBusPtr aBus) {
        if (!aBus) {
            return;
        }

        sd_bus_ref(aBus);
    }

    Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg) {
        if (!aBus || !aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_send(aBus, aMsg, nullptr));
    }

    Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg, std::string_view aDestination) {
        if (!aBus || !aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_send_to(aBus, aMsg, aDestination.data(), nullptr));
    }

    Status callSync(RawBusPtr aBus, RawBusMessagePtr aMsg,
        uint64_t aTimeoutUmsc, RawBusErrorPtr aErr, RawBusMessagePtr& aRep) {
        if (!aBus || !aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_call(aBus, aMsg, aTimeoutUmsc, aErr, &aRep));
    }

    Status callAsync(RawBusPtr aBus, RawBusSlotPtr aSlot, RawBusMessagePtr aMsg,
        RawBusMessageHandler aCallback, void* aUsrData, uint64_t aTimeoutUmsc) {
        if (!aBus || !aMsg) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_call_async(
            aBus, &aSlot, aMsg, aCallback, aUsrData, aTimeoutUmsc
        ));
    }

    void closeBus(RawBusPtr aBus) {
        if (!aBus) {
            return;
        }

        if (isBusReady(aBus)) {
            flushBus(aBus);
        }

        if (isBusOpen(aBus)) {
            sd_bus_close(aBus);
        }

        sd_bus_unref(aBus);
    }

    int getFd(RawBusPtr aBus) {
        if (!aBus) {
            return -1;
        }
        
        return sd_bus_get_fd(aBus);
    }

    int process(RawBusPtr aBus, RawBusMessagePtr* aMsg) {
        if (!aBus) {
            return -1;
        }

        return sd_bus_process(aBus, aMsg);
    }

    int wait(RawBusPtr aBus, uint64_t aTimeout) {
        if (!aBus) {
            return -1;
        }

        return sd_bus_wait(aBus, aTimeout);
    }

    RawBusSlotPtr getCurSlot(RawBusPtr aBus) {
        if (!aBus) {
            return nullptr;
        }

        return sd_bus_get_current_slot(aBus);
    }

    RawBusMessagePtr getCurMessage(RawBusPtr aBus) {
        if (!aBus) {
            return nullptr;
        }

        return sd_bus_get_current_message(aBus);
    }

    RawBusMessageHandler getCurMessageHandler(RawBusPtr aBus) {
        if (!aBus) {
            return nullptr;
        }

        return sd_bus_get_current_handler(aBus);
    }

    Status attachEvent(RawBusPtr aBus, RawBusEventPtr aEvent, int aPrio) {
        if (!aBus || ! aEvent) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_attach_event(aBus, aEvent, aPrio));
    }

    Status detachEvent(RawBusPtr aBus) {
        if (!aBus) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_detach_event(aBus));
    }

    RawBusEventPtr getCurEvent(RawBusPtr aBus) {
        if (!aBus) {
            return nullptr;
        }

        return sd_bus_get_event(aBus);
    }

    std::string_view getUniqueName(RawBusPtr aBus) {
        if (!aBus) {
            return "";
        }

        const char* name = nullptr;
        sd_bus_get_unique_name(aBus, &name);
        return name ? std::string_view(name) : std::string_view();
    }

    Status setUniqueName(RawBusPtr aBus, std::string_view aName, uint64_t aFlags) {
        if (!aBus) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(sd_bus_request_name(
            aBus, aName.data(), aFlags));
    }

    Status listenSignal(RawBusPtr aBus, RawBusSlotPtr& aSlot,
        std::string_view aSender, std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, RawBusMessageHandler aCallback, void* aData) {
        return RawError::makeStatus(
            sd_bus_match_signal(
                aBus, &aSlot, aSender.data(), aPath.data(), aIface.data(),
                aSignal.data(), aCallback, aData
            ));
    }

    Status addObjectToVTable(RawBusPtr aBus, RawBusSlotPtr& aSlot,
        std::string_view aPath, std::string_view aIface, Adaptor::RawBusVTable* aVTable, void* aData) {
        if (!aBus
            || !RawCheck::isPathNameValid(aPath)
            || !RawCheck::isInterfaceNameValid(aIface)
            || !aVTable) {
            return Status(StatusCode::INVALID_ARG);
        }

        return RawError::makeStatus(
            sd_bus_add_object_vtable(aBus, &aSlot, aPath.data(), aIface.data(), aVTable, aData)
        );
    }
    
}

}
}

#endif