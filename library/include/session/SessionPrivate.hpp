#ifndef SSDBUS_SESSION_PRIVATE_HPP
#define SSDBUS_SESSION_PRIVATE_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "DbusSlot.hpp"
#include "Status.hpp"
#include "Utils.hpp"

#include "adaptor/RawAdaptor.hpp"
#include "adaptor/RawBusSharePtr.hpp"
#include "message/MessagePrivate.hpp" 
#include "message/SignalHandler.hpp"

namespace SSDbus {
namespace Private {

class SessionPrivate {
public:
    struct MethodInfo {
        std::string input;
        std::string output;
        std::shared_ptr<void> method;
        std::unique_ptr<Adaptor::RawBusVTable[]> vtable;
        Slot slot;
    };

    using MethodMap = std::unordered_map<std::string, MethodInfo>;

    explicit SessionPrivate(bool aIsSystem)
        : mRawBus(Adaptor::RawBusSharePtr::make(aIsSystem)) {}

    SessionPrivate() = default;

    Adaptor::RawBusPtr rawBus() const {
        return mRawBus.get();
    }

    const ServiceInfo& info() const {
        return mInfo;
    }

    ServiceInfo& info() {
        return mInfo;
    }

    MethodMap& methods() {
        return mRegisteredMethods;
    }

    Status setInfo(ServiceInfo aInfo) {
        std::cout << "name:" << aInfo.name << ", path:" << aInfo.path << ", interface:" << aInfo.interface << std::endl;
        Status st = Adaptor::RawBus::setUniqueName(mRawBus.get(), aInfo.name.c_str(), 0);
        if (st.isSuccess()) {
            mInfo = aInfo;
        }
        else {
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
        return sendMessage(aMsg, aMsg.getSender());
    }

    Status sendMessage(MessagePrivate& aMsg, std::string_view aDestination) {
        if (!mRawBus) return Status(StatusCode::UNKNOWN_ERROR);
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

    static bool isInvalidInfo(const ServiceInfo& aInfo) {
        return Adaptor::RawCheck::isServiceNameValid(aInfo.name)
            && Adaptor::RawCheck::isPathNameValid(aInfo.path)
            && Adaptor::RawCheck::isInterfaceNameValid(aInfo.interface);
    }

    void addSignalHandler(std::shared_ptr<void> aHandler) {
        mSigHandler.push_back(std::move(aHandler));
    }

private:
    Adaptor::RawBusSharePtr mRawBus { nullptr };
    ServiceInfo mInfo;
    MethodMap mRegisteredMethods;
    std::vector<std::shared_ptr<void>> mSigHandler;
};

}
}


#endif