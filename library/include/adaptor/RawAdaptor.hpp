#ifndef SSDBUS_RAW_ADAPTOR_HPP
#define SSDBUS_RAW_ADAPTOR_HPP

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus-protocol.h>

#include <string>
#include <string_view>
#include <stdexcept>
#include <cassert>

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

namespace RawCheck {

    bool isInterfaceNameValid(const char* aName) {
        return sd_bus_interface_name_is_valid(aName) > 0;
    }

    bool isInterfaceNameValid(const std::string& aName) {
        return isInterfaceNameValid(aName.c_str());
    }

    bool isServiceNameValid(const char* aName) {
        return sd_bus_service_name_is_valid(aName) > 0;
    }

    bool isServiceNameValid(const std::string& aName) {
        return isServiceNameValid(aName.c_str());
    }

    bool isMemberNameValid(const char* aName) {
        return sd_bus_member_name_is_valid(aName) > 0;
    }

    bool isMemberNameValid(const std::string& aName) {
        return isMemberNameValid(aName.c_str());
    }

    bool isPathNameValid(const char* aName) {
        return sd_bus_object_path_is_valid(aName) > 0;
    }

    bool isPathNameValid(const std::string& aName) {
        return isPathNameValid(aName);
    }
};

namespace RawBus {
    RawBusPtr openBus() {
        RawBusPtr raw = nullptr;
        int ret = sd_bus_open(&raw);
        if (ret < 0) {
            throw DbusException(
                "Failed to open bus: ", strerror(-ret));
        }

        return raw;
    }

    RawBusPtr openSystemBus() {
        RawBusPtr raw = nullptr;
        int ret = sd_bus_open_system(&raw);
        if (ret < 0) {
            throw DbusException(
                "Failed to open system bus: ", strerror(-ret));
        }

        return raw;
    }

    RawBusPtr openUserBus() {
        RawBusPtr raw = nullptr;
        int ret = sd_bus_open_user(&raw);
        if (ret < 0) {
            throw DbusException(
                "Failed to open user bus: ", strerror(-ret));
        }

        return raw;
    }

    bool isBusReady(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_is_ready(aBus) > 0;
    }

    bool isBusOpen(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_is_open(aBus) > 0;
    }

    bool flushBus(RawBusPtr aBus) {
        assert(aBus);
        int ret = sd_bus_flush(aBus);
        //! TODO: throw exception or return status
        return ret >= 0;
    }

    void unrefBus(RawBusPtr aBus) {
        assert(aBus);
        sd_bus_unref(aBus);
    }

    void refBus(RawBusPtr aBus) {
        assert(aBus);
        sd_bus_ref(aBus);
    }

    int sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg) {
        return sd_bus_send(aBus, aMsg, nullptr);
    }

    int sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg, std::string_view aDestination) {
        return sd_bus_send_to(aBus, aMsg, aDestination.data(), nullptr);
    }

    int call(RawBusPtr aBus, RawBusMessagePtr aMsg,
        uint64_t aTimeoutUmsc, RawBusErrorPtr aErr, RawBusMessagePtr& aRep) {
        return sd_bus_call(aBus, aMsg, aTimeoutUmsc, aErr, &aRep);
    }

    void closeBus(RawBusPtr aBus) {
        assert(aBus);

        if (isBusReady(aBus)) {
            flushBus(aBus);
        }

        if (isBusOpen(aBus)) {
            sd_bus_close(aBus);
        }

        sd_bus_unref(aBus);
    }

    int getFd(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_get_fd(aBus);
    }

    int process(RawBusPtr aBus, RawBusMessagePtr* aMsg) {
        assert(aBus);
        return sd_bus_process(aBus, aMsg);
    }

    int wait(RawBusPtr aBus, uint64_t aTimeout) {
        assert(aBus);
        return sd_bus_wait(aBus, aTimeout);
    }

    RawBusSlotPtr getCurSlot(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_get_current_slot(aBus);
    }

    RawBusMessagePtr getCurMessage(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_get_current_message(aBus);
    }

    RawBusMessageHandler getCurMessageHandler(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_get_current_handler(aBus);
    }

    int attachEvent(RawBusPtr aBus, RawBusEventPtr aEvent, int aPrio) {
        assert(aBus);
        assert(aEvent);
        return sd_bus_attach_event(aBus, aEvent, aPrio);
    }

    int detachEvent(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_detach_event(aBus);
    }

    RawBusEventPtr getCurEvent(RawBusPtr aBus) {
        assert(aBus);
        return sd_bus_get_event(aBus);
    }

    std::string_view getUniqueName(RawBusPtr aBus) {
        assert(aBus);
        const char* name = nullptr;
        sd_bus_get_unique_name(aBus, &name);
        return name ? std::string_view(name) : std::string_view();
    }

    int setUniqueName(RawBusPtr aBus, const char* aName, uint64_t aFlags) {
        assert(aBus);
        assert(aName);
        int ret = sd_bus_request_name(aBus, aName, aFlags);
        return ret;
    }

}

namespace RawSlot {
    void closeSlot(RawBusSlotPtr aSlot) {
        assert(aSlot);
        sd_bus_slot_unref(aSlot);
    }

    RawBusPtr getBus(RawBusSlotPtr aSlot) {
        assert(aSlot);
        return sd_bus_slot_get_bus(aSlot);
    }

    void* getUserdata(RawBusSlotPtr aSlot) {
        assert(aSlot);
        return sd_bus_slot_get_userdata(aSlot);
    }

    void setUserdata(RawBusSlotPtr aSlot, void* aUserdata) {
        assert(aSlot);
        sd_bus_slot_set_userdata(aSlot, aUserdata);
    }

    bool getSlotFloating(RawBusSlotPtr aSlot) {
        assert(aSlot);
        int ret = sd_bus_slot_get_floating(aSlot);
        if (ret < 0) {
            throw DbusException(
                "Failed to get slot floating: ", strerror(-ret));
        }

        return ret > 0;
    }

    void setSlotFloating(RawBusSlotPtr aSlot, bool isFloating) {
        assert(aSlot);
        int ret = sd_bus_slot_set_floating(aSlot, isFloating);
        if (ret < 0) {
            throw DbusException(
                "Failed to set slot floating: ", strerror(-ret));
        }
    }

    void setSlotDeleterCallback(RawBusSlotPtr aSlot, RawDeleterCallback aCallback) {
        assert(aSlot);
        int ret = sd_bus_slot_set_destroy_callback(aSlot, aCallback);
        if (ret < 0) {
            throw DbusException(
                "Failed to set slot floating: ", strerror(-ret));
        }
    }

    RawDeleterCallback getSlotDeleterCallback(RawBusSlotPtr aSlot) {
        assert(aSlot);
        RawDeleterCallback callback = nullptr;
        sd_bus_slot_get_destroy_callback(aSlot, &callback);
        return callback;
    }

    RawBusMessagePtr getCurMessage(RawBusSlotPtr aSlot) {
        assert(aSlot);
        return sd_bus_slot_get_current_message(aSlot);
    }

    RawBusMessageHandler getCurMessageHandler(RawBusSlotPtr aSlot) {
        assert(aSlot);
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

    RawBusMessagePtr createSignal(
        RawBusPtr aBus, const char* aPath, const char *aInterface, const char* aMember) {
        assert(aBus);
        if (!RawCheck::isPathNameValid(aPath)
            || !RawCheck::isInterfaceNameValid(aInterface)
            || !RawCheck::isMemberNameValid(aMember)) {
            return nullptr;
        }

        RawBusMessagePtr raw = nullptr;
        sd_bus_message_new_signal(aBus, &raw, aPath, aInterface, aMember);
        return raw;
    }

    RawBusMessagePtr createSignal(
        RawBusPtr aBus, const std::string& aPath, const std::string& aInterface, const std::string& aMember) {
        return createSignal(aBus, aPath.c_str(), aInterface.c_str(), aMember.c_str());
    }

    RawBusMessagePtr createMethodCall(
        RawBusPtr aBus, const char* aDestination, const char* aPath, const char *aInterface, const char* aMember) {
        assert(aBus);
        if (!RawCheck::isServiceNameValid(aDestination)
            || !RawCheck::isPathNameValid(aPath)
            || !RawCheck::isInterfaceNameValid(aInterface)
            || !RawCheck::isMemberNameValid(aMember)) {
            return nullptr;
        }

        RawBusMessagePtr raw = nullptr;
        sd_bus_message_new_method_call(aBus, &raw,
            aDestination, aPath, aInterface, aMember);
        return raw;
    }

    RawBusMessagePtr createMethodCall(
        RawBusPtr aBus, const std::string& aDestination, const std::string& aPath, const std::string& aInterface, const std::string& aMember) {
        return createMethodCall(aBus, aDestination.c_str(), aPath.c_str(), aInterface.c_str(), aMember.c_str());
    }

    RawBusMessagePtr createMethodReturn(RawBusMessagePtr aCall) {
        assert(aCall);
        RawBusMessagePtr raw = nullptr;
        sd_bus_message_new_method_return(aCall, &raw);
        return raw;
    }

    RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, const RawBusErrorPtr aErr) {
        assert(aCall);
        assert(aErr);
        RawBusMessagePtr raw = nullptr;
        sd_bus_message_new_method_error(aCall, &raw, aErr);
        return raw;
    }

    RawBusMessagePtr createMethodError(RawBusMessagePtr aCall, int aErrno) {
        assert(aCall);
        RawBusMessagePtr raw = nullptr;
        sd_bus_message_new_method_errno(aCall, &raw, aErrno, nullptr);
        return raw;
    }

    void closeMessage(RawBusMessagePtr aMsg) {
        assert(aMsg);
        sd_bus_message_unref(aMsg);
    }

    bool sealMessage(RawBusMessagePtr aMsg, uint64_t aCookie, uint64_t aTimeout) {
        assert(aMsg);
        int ret = sd_bus_message_seal(aMsg, aCookie, aTimeout);
        return ret >= 0;
    }

    Type getType(RawBusMessagePtr aMsg) {
        assert(aMsg);
        uint8_t type;
        int ret = sd_bus_message_get_type(aMsg, &type);
        if (ret < 0) {
            throw DbusException(
                "Failed to get message type: ", strerror(-ret));
        }

        static constexpr auto minimum = static_cast<uint8_t>(Type::Invalid);
        static constexpr auto maximum = static_cast<uint8_t>(Type::Max);
        if (type <= minimum || type >= maximum) {
            throw DbusException("Invalid message type: " + std::to_string(type));
        }

        return static_cast<Type>(type);
    }

    std::string_view getSignature(RawBusMessagePtr aMsg, bool aComplete = true) {
        assert(aMsg);
        const char* sig = sd_bus_message_get_signature(aMsg, aComplete);
        return sig ? std::string_view(sig) : std::string_view();
    }

    std::string_view getPath(RawBusMessagePtr aMsg) {
        assert(aMsg);
        const char* path = sd_bus_message_get_path(aMsg);
        return path ? std::string_view(path) : std::string_view();
    }

    std::string_view getInterface(RawBusMessagePtr aMsg) {
        assert(aMsg);
        const char* interface = sd_bus_message_get_interface(aMsg);
        return interface ? std::string_view(interface) : std::string_view();
    }

    std::string_view getMember(RawBusMessagePtr aMsg) {
        assert(aMsg);
        const char* member = sd_bus_message_get_member(aMsg);
        return member ? std::string_view(member) : std::string_view();
    }

    std::string_view getDestination(RawBusMessagePtr aMsg) {
        assert(aMsg);
        const char* destination = sd_bus_message_get_destination(aMsg);
        return destination ? std::string_view(destination) : std::string_view();
    }

    std::string_view getSender(RawBusMessagePtr aMsg) {
        assert(aMsg);
        const char* sender = sd_bus_message_get_sender(aMsg);
        return sender ? std::string_view(sender) : std::string_view();
    }

    const RawBusError* getError(RawBusMessagePtr aMsg) {
        assert(aMsg);
        return sd_bus_message_get_error(aMsg);
    }

    RawBusPtr getBus(RawBusMessagePtr aMsg) {
        assert(aMsg);
        return sd_bus_message_get_bus(aMsg);
    }

    RawBusCredsPtr getCreds(RawBusMessagePtr aMsg) {
        assert(aMsg);
        return sd_bus_message_get_creds(aMsg);
    }

    bool isSignal(
        RawBusMessagePtr aMsg, const char* aInterface = nullptr, const char* aMember = nullptr) {
        assert(aMsg);
        return sd_bus_message_is_signal(aMsg, aInterface, aMember) > 0;
    }

    bool isSignal(
        RawBusMessagePtr aMsg, const std::string& aInterface, const std::string& aMember) {
        assert(aMsg);
        return isSignal(aMsg, aInterface.c_str(), aMember.c_str()) > 0;
    }

    bool isMethodCall(
        RawBusMessagePtr aMsg, const char* aInterface = nullptr, const char* aMember = nullptr) {
        assert(aMsg);
        return sd_bus_message_is_method_call(aMsg, aInterface, aMember) > 0;
    }

    bool isMethodCall(
        RawBusMessagePtr aMsg, const std::string& aInterface, const std::string& aMember) {
        assert(aMsg);
        return isMethodCall(aMsg, aInterface.c_str(), aMember.c_str());
    }

    bool isMethodError(RawBusMessagePtr aMsg, const char* aName) {
        assert(aMsg);
        return sd_bus_message_is_method_error(aMsg, aName) > 0;
    }

    bool isMethodError(RawBusMessagePtr aMsg, const std::string& aName) {
        assert(aMsg);
        return isMethodError(aMsg, aName.c_str());
    }

    bool isEmpty(RawBusMessagePtr aMsg) {
        assert(aMsg);
        return sd_bus_message_is_empty(aMsg) > 0;
    }

    bool hasSignature(RawBusMessagePtr aMsg, const char* aSignature) {
        assert(aMsg);
        assert(aSignature);
        return sd_bus_message_has_signature(aMsg, aSignature) > 0;
    }

    bool hasSignature(RawBusMessagePtr aMsg, const std::string& aSignature) {
        return hasSignature(aMsg, aSignature.c_str());
    }

    bool setDestination(RawBusMessagePtr aMsg, const char* aDestination = nullptr) {
        assert(aMsg);
        return sd_bus_message_set_destination(aMsg, aDestination) >= 0;
    }

    bool setDestination(RawBusMessagePtr aMsg, const std::string& aDestination = nullptr) {
        return setDestination(aMsg, aDestination.c_str());
    }

    bool setSender(RawBusMessagePtr aMsg, const char* aSender = nullptr) {
        assert(aMsg);
        return sd_bus_message_set_sender(aMsg, aSender) >= 0;
    }

    bool setSender(RawBusMessagePtr aMsg, const std::string& aSender) {
        assert(aMsg);
        return setSender(aMsg, aSender.c_str());
    }

    template<typename... Values>
    int append(RawBusMessagePtr aMsg, const char* aTypes, Values&&... aValues) {
        assert(aMsg);
        int ret = sd_bus_message_append(aMsg, aTypes, std::forward<Values>(aValues)...);
        return ret;
    }

    template<typename... Values>
    int append(RawBusMessagePtr aMsg, const std::string& aTypes, Values&&... aValues) {
        return append(aMsg, aTypes, std::forward<Values>(aValues)...);
    }

    template<typename T>
    int appendBasic(RawBusMessagePtr aMsg, char aType, T&& aValue) {
        assert(aMsg);
        return sd_bus_message_append_basic(aMsg, aType, std::forward<T>(aValue));
    }

    template<typename... Values>
    int pop(RawBusMessagePtr aMsg, const char* aTypes, Values&... aValues) {
        assert(aMsg);
        int ret = sd_bus_message_read(aMsg, aTypes, &aValues...);
        return ret;
    }

    template<typename... Values>
    int pop(RawBusMessagePtr aMsg, const std::string& aTypes, Values&... aValues) {
        return pop(aMsg, aTypes.c_str(), aValues...);
    }

    template<typename T>
    int popBasic(RawBusMessagePtr aMsg, char aType, T& aValue) {
        assert(aMsg);
        return sd_bus_message_read_basic(aMsg, aType, &aValue);
    }

    std::string nextType(RawBusMessagePtr aMsg) {
        assert(aMsg);
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
        assert(aMsg);
        int ret = sd_bus_message_at_end(aMsg, aComplete);
        if (ret < 0) {
            throw DbusException(
                "Failed to judge message end: ", strerror(-ret));
        }

        return ret > 0;
    }

    RawBusMessagePtr copyMessage(RawBusMessagePtr aMsg, bool aIsAll = true) {
        assert(aMsg);
        RawBusMessagePtr copy = nullptr;
        sd_bus_message_copy(aMsg, copy, aIsAll);
        return copy;
    }

    void unrefMessage(RawBusMessagePtr aMsg) {
        assert(aMsg);
        sd_bus_message_unref(aMsg);
    }

    void refMessage(RawBusMessagePtr aMsg) {
        assert(aMsg);
        sd_bus_message_ref(aMsg);
    }

}
}
}

#endif