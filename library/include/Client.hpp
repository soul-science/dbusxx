#ifndef SSDBUS_CLIENT_HPP
#define SSDBUS_CLIENT_HPP

#include <memory>
#include <mutex>
#include <future>
#include <string>
#include <thread>

#include "Looper.hpp"
#include "Session.hpp"
#include "Reply.hpp"
#include "Utils.hpp"

namespace SSDbus {

class Client {
struct AsyncPool {
    Session session;
    Looper looper;
    std::thread thread;

    explicit AsyncPool(Session s)
        : session(std::move(s))
        , looper(session)
        , thread(&Looper::run, &looper) {}

    ~AsyncPool() {
        looper.stop();
        thread.join();
    }
};

public:
    Client() = default;

    explicit Client(SessionType aType, std::string aService,
        std::string aPath, std::string aInterface)
        : mAsyncPool(getAsyncPool(aType, aService))
        , mAsyncPtr(&mAsyncPool->session)
        , mLooper(&mAsyncPool->looper)
        , mType(aType)
        , mInfo({aService, aPath, aInterface}) {}

    explicit Client(Looper& aLooper, std::string aService,
        std::string aPath, std::string aInterface)
        : mInfo({aService, aPath, aInterface})
        , mLooper(&aLooper)
        , mAsyncPtr(aLooper.session())
        , mType(aLooper.session()->type())
        , mServiceName(aService) {}

    explicit Client(Looper& aLooper, ServiceInfo aInfo)
        : mInfo(aInfo)
        , mLooper(&aLooper)
        , mAsyncPtr(aLooper.session())
        , mType(aLooper.session()->type())
        , mServiceName(aInfo.name) {}

    ~Client() = default;

    Client(Client&& aOther) noexcept
        : mAsyncPool(std::move(aOther.mAsyncPool))
        , mAsyncPtr(aOther.mAsyncPtr)
        , mLooper(aOther.mLooper)
        , mType(aOther.mType)
        , mInfo(std::move(aOther.mInfo))
        , mServiceName(aOther.mServiceName) {
        aOther.mLooper = nullptr;
        aOther.mAsyncPtr = nullptr;
    }

    Client& operator=(Client&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        mAsyncPool = std::move(aOther.mAsyncPool);
        mAsyncPtr = aOther.mAsyncPtr;
        mLooper = aOther.mLooper;
        mType = aOther.mType;
        mInfo = std::move(aOther.mInfo);
        mServiceName = aOther.mServiceName;

        aOther.mLooper = nullptr;
        aOther.mAsyncPtr = nullptr;
        return *this;
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs) {
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->callSync<Ret, TimeoutUsec>(
                mInfo.name, mInfo.path, mInfo.interface, aMethod, aArgs...);
        }

        std::promise<Reply<Ret>> promise;
        std::future<Reply<Ret>> future = promise.get_future();
        mLooper->post(
            [this, &promise, method = std::string(aMethod),
             aArgs = std::make_tuple(aArgs...)] () mutable -> void {
                std::apply([this, &promise, &method](auto&&... aUnpack) {
                    promise.set_value(mAsyncPtr->callSync<Ret, TimeoutUsec>(
                        mInfo.name, mInfo.path, mInfo.interface, method, aUnpack...));
            }, aArgs);
        });

        return future.get();
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs) {
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->callAsync<Ret, TimeoutUsec>(
                mInfo.name, mInfo.path, mInfo.interface, aMethod, aArgs...);
        }
        
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
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->callAsync<Ret, TimeoutUsec>(
                mInfo.name, mInfo.path, mInfo.interface, aMethod,
                std::forward<Callback>(aCallback), aArgs...);
        }

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
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->listenSignal(
                mInfo.name, mInfo.path, mInfo.interface,
                aSignal, std::forward<Callback>(aCallback));
        }

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
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->listenSignal(
                mInfo.name, mInfo.path, mInfo.interface,
                aSignal, aCls, aFunc);
        }

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
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->getRemoteProperty<Ret>(
                mInfo.name, mInfo.path, mInfo.interface, aProp);
        }

        std::promise<Reply<Ret>> promise;
        std::future<Reply<Ret>> future = promise.get_future();
        mLooper->post(
            [this, &promise,
             prop = std::string(aProp)] () mutable -> void {
            promise.set_value(mAsyncPtr->getRemoteProperty<Ret>(
                mInfo.name, mInfo.path, mInfo.interface, prop
            ));
        });

        return future.get();
    }

    template<typename T>
    Status setProperty(std::string_view aProp, const T& aValue) {
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->setRemoteProperty<T>(
                mInfo.name, mInfo.path, mInfo.interface,
                aProp, aValue);
        }

        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper->post(
            [this, &promise, prop = std::string(aProp),
             value = aValue] () mutable -> void {
            promise.set_value(mAsyncPtr->setRemoteProperty<T>(
                mInfo.name, mInfo.path, mInfo.interface,
                prop, value
            ));
        });

        return future.get();
    }

    template<typename Callback>
    Status onPropertyChanged(std::string_view aProp, Callback&& aCallback) {
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->onRemotePropertyChanged<>(
                mInfo.name, mInfo.path, mInfo.interface,
                aProp, std::forward<Callback>(aCallback));
        }

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
    static Session createSession(SessionType aType, std::string_view aSocket) {
        switch (aType) {
            case SessionType::SYSTEM:
                return Session::systemSession();
            case SessionType::PEER:
                return Session::peerSession(aSocket);
            case SessionType::USER:
            default:
                return Session::userSession();
        }
    }

    static std::shared_ptr<AsyncPool> getAsyncPool(
        SessionType aType, std::string_view aSocket = "") {
        // 函数内 static —— 懒初始化，线程安全
        static std::weak_ptr<AsyncPool> sUserPool;
        static std::weak_ptr<AsyncPool> sSystemPool;
        static std::mutex sPeerLock;
        static std::map<std::string, std::weak_ptr<AsyncPool>> sPeerPools;

        if (aType == SessionType::PEER) {
            std::lock_guard lock(sPeerLock);
            auto& weak = sPeerPools[aSocket.data()];
            auto sp = weak.lock();
            if (!sp) {
                weak = sp = std::make_shared<AsyncPool>(
                    createSession(aType, aSocket));
            }
            return sp;
        }

        auto& weak = (aType == SessionType::SYSTEM) ? sSystemPool : sUserPool;
        auto sp = weak.lock();
        if (!sp) {
            weak = sp = std::make_shared<AsyncPool>(
                createSession(aType, aSocket));
        }
        return sp;
    }

    std::shared_ptr<AsyncPool> mAsyncPool { nullptr };
    Session* mAsyncPtr { nullptr };
    Looper* mLooper { nullptr };
    SessionType mType { SessionType::INVALID };
    ServiceInfo mInfo;
    std::string mServiceName;
};

}

#endif