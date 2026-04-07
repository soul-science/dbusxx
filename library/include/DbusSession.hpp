
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <systemd/sd-bus.h>
#include <utility>
#include <memory>
#include <unordered_map>
#include <vector>

#include "DbusError.hpp"
#include "DbusMessage.hpp"
#include "DbusSlot.hpp"
#include "DbusArgs.hpp"
#include "DbusReturnStatus.hpp"

#include <iostream>

namespace SSDbus {

class DbusSession {
    using Status = DbusReturnStatus::Status;

    struct MethodInfo {
        std::string input;
        std::string output;
        std::shared_ptr<void> method;
        std::unique_ptr<sd_bus_vtable[]> vtable;
        DbusSlot slot;
    };

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

    explicit DbusSession(sd_bus* aRawBus, bool aIsOwned = false)
        : mRawBus(aRawBus)
        , mIsOwned(aIsOwned) {}

    ~DbusSession() {
        if (mRawBus && mIsOwned) {
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

            DbusMessage message(aMsg);
            DbusMessage reply = DbusMessage::createReply(message);

            if constexpr (sizeof...(Args)) {
                //! Parse args from message
                std::tuple<typename ArgTypeAdaptor<Args...>::type> tpl;
                message.read(tpl);

                //! Apply function
                if constexpr (std::is_same_v<Ret, void>) {
                    std::apply(
                        [&](auto&&... aArgs) -> void {
                            (obj->*func)(std::forward<decltype(aArgs)>(aArgs)...);
                        },
                        tpl
                    );

                } else {
                    Ret res = std::apply(
                        [&](auto&&... aArgs) -> Ret {
                            return (obj->*func)(std::forward<decltype(aArgs)>(aArgs)...);
                        },
                        tpl
                    );

                    reply.write(res);
                }
            } else {
                //! Apply function
                if constexpr (std::is_same_v<Ret, void>) {
                    (obj->*func)();
                } else {
                    Ret res = (obj->*func)();
                    reply.write(res);
                }
            }

            //! Response reply
            auto ret = message.getDbus()->sendMessage(reply, message.getSender());
            return ret ? 1 : -1;
        }
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

        mRegisteredMethods[aFuncName] = {};
        auto& info = mRegisteredMethods[aFuncName];

        info.input = getArgsString(aObj, aFunc);
        info.output = getReturnString(aObj, aFunc);

        std::cout << "input: " << info.input << ", output:" << info.output << std::endl;

        //! Create vtable
        using wrapper = IfaceWrapper<Cls, Ret, Args...>;
        auto vtable = std::unique_ptr<sd_bus_vtable[]>( new sd_bus_vtable[3] {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD(aFuncName, info.input.c_str(), info.output.c_str(), &wrapper::call, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        });

        sd_bus_vtable* vtablePtr = vtable.get();
        sd_bus_slot* rawSlot { nullptr };
        auto ret = sd_bus_add_object_vtable(
            mRawBus, &rawSlot, mInfo.path.c_str(), mInfo.interface.c_str(), vtablePtr, dataPtr
        );

        DbusSlot slot(rawSlot);
        if (ret < 0) {
            mRegisteredMethods.erase(aFuncName);
            return DbusReturnStatus(
                Status::FAIL
                // DbusError("", strerror(-ret))
            );
        }

        info.method = data;
        info.vtable = std::move(vtable);
        info.slot = std::move(slot);

        return DbusReturnStatus(Status::SUCCESS);
    }

    DbusReturnStatus sendMessage(DbusMessage& aMsg) {
        return sendMessage(aMsg, aMsg.getSender());
    }

    DbusReturnStatus sendMessage(DbusMessage& aMsg, const char* aDestination) {
        if (!mRawBus) {
            return DbusReturnStatus(Status::FAIL);
        }

        int ret = sd_bus_send_to(mRawBus, aMsg.getRawPtr(), aDestination, nullptr);

        return DbusReturnStatus(ret >= 0 ? Status::SUCCESS : Status::FAIL);
    }

    // 处理事件（非阻塞）
    int process() {
        return sd_bus_process(mRawBus, nullptr);
    }

    // 等待事件（阻塞）
    int wait(uint64_t timeout_usec = UINT64_MAX) {
        return sd_bus_wait(mRawBus, timeout_usec);
    }

    // 获取文件描述符（用于集成外部事件循环）
    int getFd() const {
        return sd_bus_get_fd(mRawBus);
    }

    // 刷新（发送所有待发送消息）
    void flush() {
        sd_bus_flush(mRawBus);
    }

private:
    sd_bus* mRawBus { nullptr };

    bool mIsOwned { false };

    SessionInfo mInfo;

    std::unordered_map<std::string, MethodInfo> mRegisteredMethods;

};
}

#endif
