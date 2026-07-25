#ifndef SSDBUS_DBUS_SERVER_HPP
#define SSDBUS_DBUS_SERVER_HPP

#include <string_view>
#include <tuple>

#include "Looper.hpp"
#include "Session.hpp"
#include "Status.hpp"
#include "Utils.hpp"

namespace SSDbus {

template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    Server() = delete;

    explicit Server(ServiceInfo aInfo, bool aIsSystem = false)
        : mSession(aIsSystem)
        , mLooper(mSession)
        , mStatus(mSession.setInfo(aInfo)) {}

    explicit Server(std::string aName, std::string aPath,
        std::string aInterface, bool aIsSystem = false)
        : mSession(aIsSystem)
        , mLooper(mSession)
        , mStatus(mSession.setInfo({
            aName, aPath, aInterface
        })) {}

    Server(Server&& aOther) noexcept
        : mSession(std::move(aOther.mSession))
        , mLooper(std::move(aOther.mLooper))
        , mStatus(std::move(aOther.mStatus))
        , mInited(aOther.mInited) {}

    Server& operator=(Server&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        mSession = std::move(aOther.mSession);
        mLooper = std::move(aOther.mLooper);
        mStatus = std::move(aOther.mStatus);
        mInited = std::move(aOther.mInited);
        return *this;
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run() {
        init();
        if (status().isError()) {
            return;
        }

        mLooper.run();
    }

    void stop() {
        if (status().isError()) {
            return;
        }

        mLooper.stop();
    }

    void forceStop() {
        mLooper.stop();
    }

    void post(std::function<void()> mTask) {
        mLooper.post(std::move(mTask));
    }

    template<typename... Args>
    Status emit(std::string_view aSignal, Args&&... aArgs) {
        if (mLooper.isOwnerThread()) {
            return mSession.emitSignal(aSignal, std::forward<Args>(aArgs)...);
        }

        auto argsTuple = std::make_tuple(std::forward<Args>(aArgs)...);
        mLooper.post(
            [session = mSession,
             signal = std::string(aSignal),
             args = std::move(argsTuple)] () mutable -> void {
                std::apply(
                    [&](auto&&... aUnpacked) {
                        session.emitSignal(signal, aUnpacked...);
                    }, std::move(args)
                );
            }
        );

        return Status(StatusCode::SUCCESS);
    }

    [[nodiscard]] Status status() const {
        return mStatus.isError() ? mStatus : mLooper.status();
    }

private:
    void init() {
        if (mInited || mStatus.isError()) {
            return;
        }

        mStatus = mSession.registerObject(static_cast<Derived*>(this));
        mInited = !mStatus.isError();
    }

    Session mSession;
    Looper mLooper;
    Status mStatus;

    bool mInited { false };
};

}

#endif