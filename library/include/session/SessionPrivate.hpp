#ifndef SSDBUS_SESSION_PRIVATE_HPP
#define SSDBUS_SESSION_PRIVATE_HPP

#include <string>
#include <memory>
#include <set>
#include <unordered_map>
#include <iostream>

#include "Status.hpp"
#include "Utils.hpp"

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawSlotSharePtr.hpp"
#include "adaptor/RawBusSharePtr.hpp"
#include "message/MessagePrivate.hpp" 
#include "adaptor/VTableRegistrar.hpp"

namespace SSDbus {
namespace Private {
class SessionPrivate {
public:
    struct MethodEntry {
        std::shared_ptr<void> data;
        std::string input;
        std::string output;
        Adaptor::RawBusMessageHandler callback;
    };

    struct SignalEntry {
        std::string input;
    };

    struct PropertyEntry {
        std::shared_ptr<void> data;
        std::string signature;
        Adaptor::RawBusPropertyGetter getter;
        Adaptor::RawBusPropertySetter setter;
        bool writable;
    };

    //! (path, interface) : ObjectInfo
    struct ObjectInfo {
        std::unique_ptr<Adaptor::VTableContext> context;
        std::unordered_map<std::string, MethodEntry> methods;
        std::unordered_map<std::string, SignalEntry> signals;
        std::unordered_map<std::string, PropertyEntry> properties;
    };

    struct SignalHandlerInfo {
        std::string sender;
        std::string path;
        std::string iface;
        std::string signal;
        Adaptor::RawBusMessageHandler callback;
        std::shared_ptr<void> data;
        Adaptor::RawSlotSharePtr slot;
    };

    struct PropertyHandlerInfo {
        Adaptor::RawSlotSharePtr slot;
        std::shared_ptr<void> handler; 
    };

    inline static std::string makeKey(std::string_view path, std::string_view iface) {
        std::string key;
        key.reserve(path.size() + iface.size() + 1);
        key.append(path).append(":").append(iface);
        return key;
    }

    inline static std::pair<std::string_view, std::string_view>
        parseKey(std::string_view key) {
        auto pos = key.rfind(':');
        if (pos == std::string_view::npos) {
            return {key, {}};
        }

        return {key.substr(0, pos), key.substr(pos + 1)};
    }

    using ObjectMap = std::unordered_map<std::string, ObjectInfo>;
    using PropertyHandlerMap = std::unordered_map<std::string, PropertyHandlerInfo>;
    using SignalHandlerVector = std::vector<SignalHandlerInfo>;


    SessionPrivate() = default;

    explicit SessionPrivate(SessionType aType, std::string_view aServiceName)
        : mRawBus(makePrivate(aType, aServiceName))
        , mType(aType)
        , mServiceName(aServiceName) {
        setDaemonDeathWatcher();
    }

    ~SessionPrivate() {
        if (mRawBus.get()) {
            Adaptor::RawBus::closeBus(mRawBus.get());
        }
    }

    Status reconnect() {
        //! Clear all slots
        for (auto& [name, object] : mRegisteredObjects) {
            object.context->slot = Adaptor::RawSlotSharePtr();
        }
            
        for (auto& inf : mRegisteredSigHandlers) {
            inf.slot = Adaptor::RawSlotSharePtr();
        }
            
        for (auto& [key, inf] : mRegisteredPropHandlers) {
            inf.slot = Adaptor::RawSlotSharePtr();
        }

        if (mRawBus) {
            Adaptor::RawBus::closeBus(mRawBus.get());
        }

        mRawBus = makePrivate(mType);
        if (!mRawBus) {
            return Status(StatusCode::NOT_CONNECTED);
        }

        setDaemonDeathWatcher();
        if ((mType == SessionType::USER
            || mType == SessionType::SYSTEM)
            && !mServiceName.empty()) {
            return requestNameToDaemon();
        }

        return Status(StatusCode::SUCCESS);
    }

    static bool isValidInfo(const ServiceInfo& aInfo) {
        return Adaptor::RawCheck::isServiceNameValid(aInfo.name)
            && Adaptor::RawCheck::isPathNameValid(aInfo.path)
            && Adaptor::RawCheck::isInterfaceNameValid(aInfo.interface);
    }

    Adaptor::RawBusSharePtr rawBus() const {
        return mRawBus;
    }

    const ServiceInfo& info() const {
        return mInfo;
    }

    std::string serviceName() const {
        return mServiceName;
    }

    SessionType type() const {
        return mType;
    }

    ServiceInfo& info() {
        return mInfo;
    }

    ObjectMap& objects() {
        return mRegisteredObjects;
    }

    SignalHandlerVector& signalHandlers() {
        return mRegisteredSigHandlers;
    }

    PropertyHandlerMap& propertyHandlers() {
        return mRegisteredPropHandlers;
    }

    Status requestNameToDaemon() {
        Status st = Adaptor::RawBus::setUniqueName(mRawBus.get(), mServiceName, 0);
        if (st.isError()) {
            std::cout << "setUniqueName failed, reason:" << st.message() << std::endl;
        }
        return st;
    }

    Status setInfo(ServiceInfo aInfo) {
        std::cout << "name:" << aInfo.name << ", path:" << aInfo.path << ", interface:" << aInfo.interface << std::endl;
        Status st = Adaptor::RawBus::setUniqueName(mRawBus.get(), aInfo.name.c_str(), 0);
        if (st.isSuccess()) {
            mInfo = aInfo;
        } else {
            std::cout << "setUniqueName failed, reason:" << st.message() << std::endl;
        }

        return st;
    }

    MessagePrivate createReply(MessagePrivate& aCallMsg) {
        auto reply = Adaptor::RawMessageSharePtr::createReply(aCallMsg.rawMessage());
        return MessagePrivate(reply);
    }

    MessagePrivate createMethodCall(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod) {
        
        auto call =  Adaptor::RawMessageSharePtr::createMethodCall(
            mRawBus.get(), aService, aPath, aIface, aMethod 
        );

        return MessagePrivate(call);
    }

    Status sendMessage(MessagePrivate& aMsg) {
        if (!mRawBus) {
            return Status(StatusCode::UNKNOWN_ERROR);
        }

        return Adaptor::RawBus::sendMessage(mRawBus.get(), aMsg.rawMessage());
    }

    Status sendMessage(MessagePrivate& aMsg, std::string_view aDestination) {
        if (!mRawBus) {
            return Status(StatusCode::UNKNOWN_ERROR);
        }

        return Adaptor::RawBus::sendMessage(mRawBus.get(), aMsg.rawMessage(), aDestination);
    }

    int process() {
        return Adaptor::RawBus::process(mRawBus.get(), nullptr);
    }

    int wait(uint64_t aTimeoutMs = UINT64_MAX) {
        return Adaptor::RawBus::wait(mRawBus.get(), aTimeoutMs);
    }

    int getFd() const {
        return Adaptor::RawBus::getFd(mRawBus.get());
    }

    void flush() {
        Adaptor::RawBus::flushBus(mRawBus.get());
    }

    void addSignalHandlerInfo(SignalHandlerInfo aInf) {
        mRegisteredSigHandlers.push_back(std::move(aInf));
    }

    std::shared_ptr<void> getPropertyHandler(std::string_view aKey) const {
        auto it = mRegisteredPropHandlers.find(std::string(aKey));
        if (it == mRegisteredPropHandlers.end()) { 
            return nullptr;
        }

        return it->second.handler;
    }

    void setPropertyHandlerInfo(std::string_view aKey, PropertyHandlerInfo aInf) {
        mRegisteredPropHandlers[std::string(aKey)] = std::move(aInf);
    }

private:
    static Adaptor::RawBusSharePtr makePrivate(SessionType aType,
        std::string_view aServiceName = "") {
        switch (aType) {
            case SessionType::USER:
                return Adaptor::RawBusSharePtr::makeUser();
            case SessionType::SYSTEM:
                return Adaptor::RawBusSharePtr::makeSystem();
            case SessionType::PEER:
                return Adaptor::RawBusSharePtr::makePeer(aServiceName);
            default:
                return Adaptor::RawBusSharePtr(nullptr, false);
        }
    }

    void setDaemonDeathWatcher() {
        Adaptor::RawBus::setWatchBind(mRawBus.get(), true);
        Adaptor::RawBus::setExitOnDisconnect(mRawBus.get(), false);
        Adaptor::RawBus::setConnectedSignal(mRawBus.get(), true);
    }

    Adaptor::RawBusSharePtr mRawBus { nullptr };
    SessionType mType { false };
    std::string mServiceName;

    ServiceInfo mInfo;
    // MethodMap mRegisteredMethods;
    // SignalMap mRegisteredSignals;
    // PropertyMap mRegisteredProperties;
    ObjectMap mRegisteredObjects;
    SignalHandlerVector mRegisteredSigHandlers;
    PropertyHandlerMap mRegisteredPropHandlers;
};

}
}


#endif