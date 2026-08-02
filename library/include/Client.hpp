#ifndef SSDBUS_CLIENT_HPP
#define SSDBUS_CLIENT_HPP

#include <memory>
#include <future>
#include <string>
#include <thread>

#include "Looper.hpp"
#include "Session.hpp"
#include "Reply.hpp"
#include "Utils.hpp"

namespace SSDbus {

class Client {
struct SyncPool {
    Session session;

    explicit SyncPool(bool aIsSystem)
        : session(aIsSystem) {}
};

struct AsyncPool {
    Session session;
    Looper looper;
    std::thread thread;

    explicit AsyncPool(bool aIsSystem)
        : session(aIsSystem)
        , looper(session)
        , thread(&Looper::run, &looper) {}

    ~AsyncPool() {
        looper.stop();
        thread.join();
    }
};

public:
    explicit Client(std::string aService,
        std::string aPath, std::string aInterface, bool aIsSystem = false)
        : mInfo({aService, aPath, aInterface})
        , mSyncPool(getSyncPool(aIsSystem))
        , mAsyncPool(getAsyncPool(aIsSystem))
        , mAsyncPtr(&mAsyncPool->session)
        , mLooper(&mAsyncPool->looper)
        , mIsSystem(aIsSystem) {}

    explicit Client(ServiceInfo aInfo, bool aIsSystem = false)
        : mInfo(aInfo)
        , mSyncPool(getSyncPool(aIsSystem))
        , mAsyncPool(getAsyncPool(aIsSystem))
        , mAsyncPtr(&mAsyncPool->session)
        , mLooper(&mAsyncPool->looper)
        , mIsSystem(aIsSystem) {}

    explicit Client(Looper& aLooper, std::string aService,
        std::string aPath, std::string aInterface, bool aIsSystem = false)
        : mInfo({aService, aPath, aInterface})
        , mSyncPool(getSyncPool(aIsSystem))
        , mLooper(&aLooper)
        , mAsyncPtr(aLooper.session())
        , mIsSystem(aIsSystem) {}

    explicit Client(Looper& aLooper, ServiceInfo aInfo, bool aIsSystem = false)
        : mInfo(aInfo)
        , mSyncPool(getSyncPool(aIsSystem))
        , mLooper(&aLooper)
        , mAsyncPtr(aLooper.session())
        , mIsSystem(aIsSystem) {}

    ~Client() = default;

    Client(Client&& aOther) noexcept
        : mInfo(std::move(aOther.mInfo))
        , mSyncPool(std::move(aOther.mSyncPool))
        , mAsyncPool(std::move(aOther.mAsyncPool))
        , mAsyncPtr(aOther.mAsyncPtr)
        , mLooper(aOther.mLooper)
        , mIsSystem(aOther.mIsSystem) {
        aOther.mLooper = nullptr;
        aOther.mAsyncPtr = nullptr;
    }

    Client& operator=(Client&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        mInfo = std::move(aOther.mInfo);
        mSyncPool  = std::move(aOther.mSyncPool);
        mAsyncPool = std::move(aOther.mAsyncPool);
        mAsyncPtr = aOther.mAsyncPtr;
        mLooper = aOther.mLooper;
        mIsSystem = aOther.mIsSystem;

        aOther.mLooper = nullptr;
        aOther.mAsyncPtr = nullptr;
        return *this;
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs) {
        return mSyncPool->session.callSync<Ret, TimeoutUsec>(mInfo.name, mInfo.path, mInfo.interface,
            aMethod, aArgs...);
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs) {
        std::promise<PendingReply<Ret>> promise;
        std::future<PendingReply<Ret>> future = promise.get_future();
        mLooper->post(
            [this, &promise, method = std::string(aMethod),
            aArgs = std::make_tuple(aArgs...)] () mutable -> void {
            std::apply([this, &promise, &method](auto&&... aUnpack) {
                promise.set_value(mAsyncPtr->callAsync<Ret, TimeoutUsec>(
                    mInfo.name, mInfo.path, mInfo.interface, method, aUnpack...));
            }, aArgs);
        });

        return future.get();
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
    Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper->post(
            [this, &promise, method = std::string(aMethod),
            cb = std::forward<Callback>(aCallback),
            aArgs = std::make_tuple(aArgs...)] () mutable -> void {
            std::apply([this, &promise, &method, &cb](auto&&... aUnpack) {
                promise.set_value(mAsyncPtr->callAsync<Ret, TimeoutUsec>(
                    mInfo.name, mInfo.path, mInfo.interface, method,
                    std::forward<Callback>(cb), aUnpack...));
            }, aArgs);
        });

        return future.get();
    }

    template<typename Callback>
    Status listenSignal(std::string_view aSignal, Callback&& aCallback) {
        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper->post(
            [this, &promise, signal = std::string(aSignal),
             cb = std::forward<Callback>(aCallback)] () mutable -> void {
            promise.set_value(mAsyncPtr->listenSignal(
                mInfo.name, mInfo.path, mInfo.interface,
                signal, std::forward<Callback>(cb)
            ));
        });

        return future.get();
    }

    template<typename Cls, typename Ret, typename... Args>
    Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper->post(
            [this, &promise, signal = std::string(aSignal),
            cls = aCls, func = aFunc] () mutable -> void {
            promise.set_value(mAsyncPtr->listenSignal(
                mInfo.name, mInfo.path, mInfo.interface,
                signal, cls, func
            ));
        });

        return future.get();
    }

    template<typename Ret>
    Reply<Ret> getProperty(std::string_view aProp) {
        return mSyncPool->session.getRemoteProperty<Ret>(
            mInfo.name, mInfo.path, mInfo.interface, aProp
        );
    }

    template<typename T>
    Status setProperty(std::string_view aProp, const T& aValue) {
        return mSyncPool->session.setRemoteProperty<>(
            mInfo.name, mInfo.path, mInfo.interface, aProp, aValue
        );
    }

    template<typename Callback>
    Status onPropertyChanged(std::string_view aProp, Callback&& aCallback) {
        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper->post(
            [this, &promise, prop = std::string(aProp),
             cb = std::forward<Callback>(aCallback)] () mutable -> void {
            promise.set_value(mAsyncPtr->onRemotePropertyChanged<>(
                mInfo.name, mInfo.path, mInfo.interface, prop,
                std::forward<Callback>(cb)
            ));
        });

        return future.get();
    }

private:
    static std::weak_ptr<SyncPool>& syncPoolweak(bool aIsSystem) {
        thread_local std::weak_ptr<SyncPool> sSyncUser;
        thread_local std::weak_ptr<SyncPool> sSyncSystem;

        return aIsSystem ? sSyncSystem : sSyncUser;
    }

    static std::weak_ptr<AsyncPool>& asyncPoolweak(bool aIsSystem) {
        thread_local std::weak_ptr<AsyncPool> sAsyncUser;
        thread_local std::weak_ptr<AsyncPool> sAsyncSystem;

        return aIsSystem ? sAsyncSystem : sAsyncUser;
    }

    static std::shared_ptr<SyncPool> getSyncPool(bool aIsSystem) {
        auto& weakPtr = syncPoolweak(aIsSystem);
        auto sharePtr = weakPtr.lock();
        if (!sharePtr) {
            weakPtr = sharePtr = std::make_shared<SyncPool>(aIsSystem);
        }

        return sharePtr;
    }

    static std::shared_ptr<AsyncPool> getAsyncPool(bool aIsSystem) {
        auto& weakPtr = asyncPoolweak(aIsSystem);
        auto sharePtr = weakPtr.lock();
        if (!sharePtr) {
            weakPtr = sharePtr = std::make_shared<AsyncPool>(aIsSystem);
        }

        return sharePtr;
    }

    ServiceInfo mInfo;
    std::shared_ptr<SyncPool> mSyncPool { nullptr };
    std::shared_ptr<AsyncPool> mAsyncPool { nullptr };

    Session* mAsyncPtr { nullptr };
    Looper* mLooper { nullptr };
    bool mIsSystem { false };
};

}

#endif