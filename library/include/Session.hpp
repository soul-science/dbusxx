
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <systemd/sd-bus.h>
#include <utility>
#include <memory>
#include <unordered_map>
#include <vector>

#include "DbusError.hpp"
#include "Message.hpp"
#include "DbusSlot.hpp"
#include "DbusArgs.hpp"
#include "DbusReturnStatus.hpp"

#include "adaptor/RawBusSharePtr.hpp"
#include "session/SessionPrivate.hpp"
#include "method/Method.hpp"

#include <iostream>

namespace SSDbus {

class Session {
    using Status = DbusReturnStatus::Status;
public:
    explicit Session(bool aIsSystem = false)
    : mPrivate(std::make_shared<Private::SessionPrivate>(aIsSystem)) {}

    ~Session() = default;

    Session(const Session& aOther) = default;
    Session& operator=(const Session& aOther) = default;

    Session(Session&& aOther) noexcept = default;
    Session& operator=(Session&& aOther) noexcept = default;

    static Session systemBus() {
        return Session(true);
    }

    static Session sessionBus() {
        return Session(false);
    }

    DbusReturnStatus setInfo(ServiceInfo aInfo) {
        return mPrivate->setInfo(aInfo);
    }

    int process() {
        return mPrivate->process();
    }

    int wait(uint64_t aTimeoutMs = UINT64_MAX) {
        return mPrivate->wait(aTimeoutMs);
    }

    int getFd() const {
        return mPrivate->getFd();
    }

    void flush() {
        mPrivate->flush();
    }

    template<typename Cls, typename Ret, typename... Args>
    DbusReturnStatus registerInterface(const char* aFuncName, Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
        return Method::registerMethod(mPrivate.get(), aFuncName, aObj, aFunc);
    }

    template<typename Ret, typename... Args>
    DbusReturnStatus callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Method::callSync<Ret>(mPrivate.get(), aService, aPath, aIface, aMethod, aArgs...);
    }

private:
    std::shared_ptr<Private::SessionPrivate> mPrivate { nullptr };
};

}

#endif
