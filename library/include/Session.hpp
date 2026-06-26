
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <systemd/sd-bus.h>
#include <utility>
#include <memory>
#include <unordered_map>
#include <vector>
#include <type_traits>

#include "DbusError.hpp"
#include "Message.hpp"
#include "Reply.hpp"
#include "PendingReply.hpp"
#include "DbusSlot.hpp"
#include "DbusArgs.hpp"
#include "Status.hpp"

#include "adaptor/RawBusSharePtr.hpp"
#include "session/SessionPrivate.hpp"
#include "method/Method.hpp"

#include <iostream>

namespace SSDbus {

class Session {
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

    Status setInfo(ServiceInfo aInfo) {
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
    Status registerInterface(const char* aFuncName, Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
        return Method::registerMethod(mPrivate.get(), aFuncName, aObj, aFunc);
    }

    template<typename Ret, typename... Args>
    Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, uint64_t aTimeoutUmsc, const Args&... aArgs) {
        return Reply<Ret>(
            Method::callSync<Ret, Args...>(
                mPrivate.get(), aTimeoutUmsc, aService, aPath, aIface, aMethod, aArgs...
            ));
    }

    template<typename... Args>
    Reply<void> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, uint64_t aTimeoutUmsc, const Args&... aArgs) {
        return Reply<void>(
            Method::callSync<void, Args...>(mPrivate.get(), aTimeoutUmsc, aService, aPath, aIface, aMethod, aArgs...
        ));
    }

    template<typename Ret, typename... Args>
    Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Reply<Ret>(
            Method::callSync<Ret, Args...>(
                mPrivate.get(), aService, aPath, aIface, aMethod, aArgs...
            ));
    }

    template<typename... Args>
    Reply<void> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Reply<void>(
            Method::callSync<void, Args...>(mPrivate.get(), aService, aPath, aIface, aMethod, aArgs...
        ));
    }

    template<typename Ret, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return PendingReply<Ret>(
            Method::callAsync<Ret, Args...>(mPrivate.get(), aService, aPath, aIface, aMethod, aArgs...)
        );
    }

    template<typename Ret, typename Callback, typename... Args>
    //! Callback takes precedence over template<typename Ret, typename... Args>
    Status callAsync(std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aMethod, uint64_t aTimeoutUmsc, Callback&& aCallback, const Args&... aArgs) {
        static_assert(std::is_invocable_r_v<void, Callback, Reply<Ret>>,
            "callAsync callback must be callable as: void(Reply<Ret>)");
        using Call = std::function<void(Reply<Ret>)>;
        auto rep = std::make_shared<PendingReply<Ret>>(
            Method::callAsync<Ret, Args...>(mPrivate.get(), aTimeoutUmsc, aService, aPath, aIface, aMethod, aArgs...)
        );

        mReps.push_back(rep);
        void* repPtr = rep.get();
        rep->setCallback(
            [this, cb = Call(std::forward<Callback>(aCallback)), repPtr] (Reply<Ret> aRep) {
                //! Use RAII to ensure release old rep
                struct Clear {
                    std::vector<std::shared_ptr<void>>& reps;
                    void* ptr;
                    ~Clear() {
                        auto it = std::find_if(reps.begin(), reps.end(),
                            [this](const auto& p) {
                                return p.get() == ptr;
                        });
                        if (it != reps.end()) {
                            reps.erase(it);
                        }
                    }
                } clear{mReps, repPtr};

                cb(aRep);
            }
        );

        return rep->getStatus();
    }

    template<typename Ret, typename Callback, typename... Args>
    //! Callback takes precedence over template<typename Ret, typename... Args>
    Status callAsync(std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
        return callAsync<Ret, Callback, Args...>(
            aService, aPath, aIface, aMethod, static_cast<uint64_t>(0), std::forward<Callback>(aCallback), aArgs...
        );
    }

    template<typename Callback>
    Status listenSignal(std::string_view aSender,
        std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Callback&& aCallback) {
            return Method::listenSignal(
                mPrivate.get(), aSender, aPath, aIface, aSignal,
                std::forward<Callback>(aCallback)
        );
    }

private:
    std::shared_ptr<Private::SessionPrivate> mPrivate { nullptr };
    std::vector<std::shared_ptr<void>> mReps;
};

}

#endif
