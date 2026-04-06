
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <systemd/sd-bus.h>
#include <utility>
#include <memory>
#include <unordered_map>
#include <vector>

#include <any>

#include "DbusError.hpp"
#include "DbusMessage.hpp"
#include "DbusSlot.hpp"
#include "DbusArgs.hpp"
#include "DbusReturnStatus.hpp"

#include <iostream>

namespace SSDbus {

class DbusSession {
    using Status = DbusReturnStatus::Status;
public:

    struct SessionInfo {
        std::string name;
        std::string path;
        std::string interface;
    };

    explicit DbusSession(bool aIsSystem = false) {
        int ret = aIsSystem ? sd_bus_open_system(&mRawBus)
            : sd_bus_open_user(&mRawBus);
        if (ret < 0) {
            throw DbusException("Failed to open bus: ", strerror(-ret));
        }
    }

    ~DbusSession() {
        if (mRawBus) {
            sd_bus_unref(mRawBus);
        }
    }

    DbusSession(DbusSession&& aOther) noexcept
        : mRawBus(aOther.mRawBus) {
        aOther.mRawBus = nullptr;
    }

    DbusSession& operator=(DbusSession&& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (mRawBus) {
            sd_bus_unref(mRawBus);
        }

        mRawBus = aOther.mRawBus;
        aOther.mRawBus = nullptr;
        return *this;
    }

    DbusSession(const DbusSession&) = delete;
    DbusSession& operator=(const DbusSession&) = delete;

    static DbusSession CreateSession(SessionInfo&& aInfo, bool aIsSystem = false) {
        DbusSession session(aIsSystem);
        session.setDbusInfo(std::forward<SessionInfo>(aInfo));
        return session;
    }

    DbusReturnStatus setDbusInfo(SessionInfo aInfo) {
        std::cout << "name:" << aInfo.name << ", path:" << aInfo.path << ", interface:" << aInfo.interface << std::endl;
        int ret = sd_bus_request_name(mRawBus, aInfo.name.c_str(), 0);
        if (ret < 0) {
            std::cout << "sd_bus_request_name failed, reason:" << ret << " " << strerror(-ret) << std::endl;
            return DbusReturnStatus(Status::FAIL);
        }

        mInfo = aInfo;
        return DbusReturnStatus(Status::SUCCESS);
    }

    template <typename Cls, typename Ret, typename... Args>
    struct IfaceWrapper {
        using ClsFuncPtr = Ret (Cls::*)(Args...);

        static int call(sd_bus_message* aMsg, void* aUsrData, sd_bus_error* aErr) {
            auto* pair = static_cast<std::pair<Cls*, ClsFuncPtr>*>(aUsrData);
            Cls* obj = pair->first;
            ClsFuncPtr func = pair->second;

            //! Parse args from message
            DbusMessage message(aMsg);
            std::tuple<Args...> tpl;
            message.read(tpl);

            //! Apply function
            Ret res = std::apply(
                [&](auto&&... aArgs) -> Ret {
                    return (obj->*func)(std::forward<decltype(aArgs)>(aArgs)...);
                },
                tpl
            );

            //! Response reply

            return 1;
        }
    };

    struct MethodInfo {
        std::any method; 
        std::unique_ptr<sd_bus_vtable[]> vtable;
        DbusSlot slot;
    };

    template<typename Cls, typename Ret, typename... Args>
    std::string getArgsString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
        std::string args;

        if constexpr (sizeof...(Args)) {
            (args.append(std::string(1, SSDbus::getSignature<Args>())), ...);
        }

        return args;
    }

    template<typename Cls, typename Ret, typename... Args>
    std::string getReturnString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
        return std::string(1, SSDbus::getSignature<Ret>());
    }

    template<typename Cls, typename Ret, typename... Args>
    DbusReturnStatus registerInterface(const char* aFuncName, Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
        using ClsFuncPtr = Ret (Cls::*)(Args...);

        if (mInfo.path.empty() || mInfo.path.empty()) {
            return DbusReturnStatus(
                Status::FAIL
                // DbusError("", "service name or path is empty")
            );
        }

        auto data = std::make_shared<std::pair<Cls*, ClsFuncPtr>>(aObj, aFunc);
        auto dataPtr = data.get();

        //! Create vtable
        using wrapper = IfaceWrapper<Cls, Ret, Args...>;
        std::string input = getArgsString(aObj, aFunc);
        std::string output = getReturnString(aObj, aFunc);

        std::cout << "input: " << input << ", output:" << output << std::endl;

        auto vtable = std::unique_ptr<sd_bus_vtable[]>( new sd_bus_vtable[3] {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD(aFuncName, input.c_str(), output.c_str(), &wrapper::call, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        });

        sd_bus_vtable* vtablePtr = vtable.get();
        sd_bus_slot* rawSlot { nullptr };
        auto ret = sd_bus_add_object_vtable(
            mRawBus, &rawSlot, mInfo.path.c_str(), mInfo.interface.c_str(), vtablePtr, dataPtr
        );

        DbusSlot slot(rawSlot);
        if (ret < 0) {
            return DbusReturnStatus(
                Status::FAIL
                // DbusError("", strerror(-ret))
            );
        }

        mRegisteredMethods[aFuncName] = { std::any(data), std::move(vtable), std::move(slot) };
        return DbusReturnStatus(Status::SUCCESS);
    }

private:
    sd_bus* mRawBus { nullptr };

    SessionInfo mInfo;

    std::unordered_map<std::string, MethodInfo> mRegisteredMethods;

};
}

#endif
