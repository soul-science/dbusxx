#ifndef SSDBUS_DBUS_INTERFACE_HPP
#define SSDBUS_DBUS_INTERFACE_HPP

#include <systemd/sd-bus.h>
#include <string>

#include "DbusSession.hpp"
#include "DbusMessage.hpp"
#include "DbusArgs.hpp"

namespace SSDbus {

class DbusInterface {
public:
    explicit DbusInterface(std::string aServiceName, std::string aServicePath,
        std::string aServiceInterface, Session aSession = Session());

    ~DbusInterface() = default;

    template<typename... Args>
    Message call(std::string aFuncName, Args... aArgs);

private:
    std::string mServiceName;
    std::string mServicePath;
    std::string mServiceInterface;
    Session mSession;
};

DbusInterface::DbusInterface(std::string aServiceName,
    std::string aServicePath, std::string aServiceInterface, Session aSession)
    : mServiceName(std::move(aServiceName))
    , mServicePath(std::move(aServicePath))
    , mServiceInterface(std::move(aServiceInterface))
    , mSession(std::move(aSession)) {}

template<typename... Args>
Message DbusInterface::call(std::string aFuncName, Args... aArgs) {
    sd_bus_message* rawReply = nullptr;
    sd_bus_error rawErr = SD_BUS_ERROR_NULL;
    auto ret = sd_bus_call_method(
        mSession.getRawPtr(),
        mServiceName.c_str(),
        mServicePath.c_str(),
        mServiceInterface.c_str(),
        aFuncName.c_str(),
        &rawErr,
        &rawReply,
        getArgsString<Args...>().c_str(),
        std::forward<Args>(aArgs)...
    );

    Message reply;

    if (ret < 0) {
        return Message();
    } else {
        reply = std::move(Message(rawReply));
        DbusError err(&rawErr);
    }

    return reply;
}

}
#endif