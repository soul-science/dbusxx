#ifndef SSDBUS_CLIENT_HPP
#define SSDBUS_CLIENT_HPP

#include <string>
#include <thread>

#include "Looper.hpp"
#include "Session.hpp"
#include "Reply.hpp"
#include "Utils.hpp"

namespace SSDbus {

class Client {
public:
    explicit Client(std::string aService,
        std::string aPath, std::string aInterface, bool aIsSystemd = false)
        : mInfo({aService, aPath, aInterface})
        , mAsyncPtr(new Session(aIsSystemd))
        , mOwnedLooper(*mAsyncPtr)
        , mIsSystem(aIsSystemd)
        , mSelfOwned(true) {
            mLoopThread = std::thread(&Looper::run, mOwnedLooper);
    }

    explicit Client(ServiceInfo aInfo, bool aIsSystemd = false)
        : mInfo(aInfo)
        , mAsyncPtr(new Session(aIsSystemd))
        , mOwnedLooper(*mAsyncPtr)
        , mIsSystem(aIsSystemd)
        , mSelfOwned(true) {
            mLoopThread = std::thread(&Looper::run, mOwnedLooper);
    }

    explicit Client(Session& aAsyncSession, std::string aService,
        std::string aPath, std::string aInterface, bool aIsSystemd = false)
        : mInfo({aService, aPath, aInterface})
        , mOwnedLooper()
        , mAsyncPtr(&aAsyncSession)
        , mIsSystem(aIsSystemd)
        , mSelfOwned(false) {}

    explicit Client(Session& aAsyncSession, ServiceInfo aInfo, bool aIsSystemd = false)
        : mInfo(aInfo)
        , mOwnedLooper()
        , mAsyncPtr(&aAsyncSession)
        , mIsSystem(aIsSystemd)
        , mSelfOwned(false) {}

    ~Client() {
        if (mSelfOwned) {
            mOwnedLooper.stop();
            if (mLoopThread.joinable()) {
                mLoopThread.join();
            }

            delete mAsyncPtr;
        }
    }

    Client(Client&& aOther) noexcept
        : mInfo(std::move(aOther.mInfo))
        , mAsyncPtr(aOther.mAsyncPtr)
        , mOwnedLooper(std::move(aOther.mOwnedLooper))
        , mLoopThread(std::move(aOther.mLoopThread))
        , mSelfOwned(aOther.mSelfOwned)
        , mIsSystem(aOther.mIsSystem) {
        aOther.mSelfOwned = false;
        aOther.mAsyncPtr = nullptr;
    }

    Client& operator=(Client&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        if (mSelfOwned) {
            mOwnedLooper.stop();
            if (mLoopThread.joinable()) mLoopThread.join();
            delete mAsyncPtr;
        }

        mInfo = std::move(aOther.mInfo);
        mAsyncPtr = aOther.mAsyncPtr;
        mOwnedLooper = std::move(aOther.mOwnedLooper);
        mLoopThread = std::move(aOther.mLoopThread);
        mSelfOwned = aOther.mSelfOwned;
        mIsSystem = aOther.mIsSystem;

        aOther.mSelfOwned = false;
        aOther.mAsyncPtr = nullptr;
        return *this;
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs) {
        return syncSession().callSync<Ret, TimeoutUsec>(mInfo.name, mInfo.path, mInfo.interface,
            aMethod, aArgs...);
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs) {
        return mAsyncPtr->callAsync<Ret, TimeoutUsec>(
            mInfo.name, mInfo.path, mInfo.interface, aMethod, aArgs...);
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
    Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
        return mAsyncPtr->callAsync<Ret, TimeoutUsec>(
            mInfo.name, mInfo.path, mInfo.interface, aMethod,
            std::forward<Callback>(aCallback), aArgs...);
    }

    template<typename Callback>
    Status listenSignal(std::string_view aSignal, Callback&& aCallback) {
        return mAsyncPtr->listenSignal(
            mInfo.name, mInfo.path, mInfo.interface, aSignal, std::forward<Callback>(aCallback)
        );
    }

    template<typename Cls, typename Ret, typename... Args>
    Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return mAsyncPtr->listenSignal(
            mInfo.name, mInfo.path, mInfo.interface, aSignal, aCls, aFunc
        );
    }

private:
    Session& syncSession() {
        thread_local Session sSystemSession(true);
        thread_local Session sUserSession(false);

        return mIsSystem ? sSystemSession : sUserSession;
    }

    ServiceInfo mInfo;
    Session* mAsyncPtr { nullptr };
    Looper mOwnedLooper;
    std::thread mLoopThread;
    bool mSelfOwned { false };
    bool mIsSystem { false };
};

}

#endif