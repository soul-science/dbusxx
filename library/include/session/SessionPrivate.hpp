#ifndef SSDBUS_SESSION_PRIVATE_HPP
#define SSDBUS_SESSION_PRIVATE_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "DbusSlot.hpp"
#include "DbusReturnStatus.hpp"
#include "Utils.hpp"

#include "adaptor/RawAdaptor.hpp"
#include "adaptor/RawBusSharePtr.hpp"
#include "message/MessagePrivate.hpp" 

namespace SSDbus {
namespace Private {

class SessionPrivate {
    using Status = SSDbus::DbusReturnStatus::Status;
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

    DbusReturnStatus setInfo(ServiceInfo aInfo) {
        std::cout << "name:" << aInfo.name << ", path:" << aInfo.path << ", interface:" << aInfo.interface << std::endl;
        int ret = Adaptor::RawBus::setUniqueName(mRawBus.get(), aInfo.name.c_str(), 0);
        if (ret < 0) {
            std::cout << "setUniqueName failed, reason:" << ret << " " << strerror(-ret) << std::endl;
            return DbusReturnStatus(Status::FAIL);
        }

        mInfo = aInfo;
        return DbusReturnStatus(Status::SUCCESS);
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

    DbusReturnStatus sendMessage(MessagePrivate& aMsg) {
        return sendMessage(aMsg, aMsg.getSender());
    }

    DbusReturnStatus sendMessage(MessagePrivate& aMsg, std::string_view aDestination) {
        if (!mRawBus) return DbusReturnStatus(DbusReturnStatus::Status::FAIL);
        int ret = Adaptor::RawBus::sendMessage(mRawBus.get(), aMsg.rawMessage(), aDestination);
        return DbusReturnStatus(ret >= 0 ?
            DbusReturnStatus::Status::SUCCESS : DbusReturnStatus::Status::FAIL);
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

private:
    Adaptor::RawBusSharePtr mRawBus { nullptr };
    ServiceInfo mInfo;
    MethodMap mRegisteredMethods;
};

}
}


#endif