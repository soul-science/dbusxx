#ifndef DBUSXX_CLIENT_HPP
#define DBUSXX_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <future>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Looper.hpp"
#include "Session.hpp"
#include "Reply.hpp"
#include "Utils.hpp"


namespace Dbusxx {
/**
 * @brief Proxy for a remote D-Bus service.
 *
 * `Client` encapsulates a session plus an event loop and exposes
 * type-safe calls, signals and properties for one remote
 * (service, path, interface). Use the four-argument constructor to let
 * the client manage its own session and loop, or the two-argument one
 * to plug into an external #Looper.
 */
class Client {
    //! True when the first argument of an async call is a completion callback
    //! (callable as void(Reply<Ret>)). Used to disambiguate the two callAsync
    //! overloads by SFINAE, mirroring Session::CallbackLikeFirstArg. Without
    //! it the callback form would win overload resolution for a plain data
    //! argument (T&& beats const Args&), making the PendingReply form
    //! unreachable and turning valid calls into hard compile errors.
    template<typename Ret, typename... Args>
    struct CallbackLikeFirstArg {
        static constexpr bool value = false;
    };

    template<typename Ret, typename First, typename... Rest>
    struct CallbackLikeFirstArg<Ret, First, Rest...> {
        static constexpr bool value =
            std::is_invocable_r_v<void, First, Reply<Ret>>;
    };

struct ServerInfo {
    std::string name;
    std::string path;
    std::string interface;
};

struct AsyncPool {
    Session session;
    Looper looper;
    std::thread thread;

    explicit AsyncPool(Session s);

    ~AsyncPool();
};

public:
    //! @brief Construct an empty (invalid) client.
    Client() = default;

    /**
     * @brief Construct a client that manages its own session and event loop.
     *
     * @param aType      session type (system/user/peer)
     * @param aService   remote service name
     * @param aPath      remote object path
     * @param aInterface remote interface name
     */
    explicit Client(SessionType aType, std::string aService,
        std::string aPath, std::string aInterface);

    /**
     * @brief Construct a client driven by an external event loop.
     *
     * @param aLooper    the loop that will drive the client
     * @param aService   remote service name
     * @param aPath      remote object path
     * @param aInterface remote interface name
     */
    explicit Client(Looper& aLooper, std::string aService,
        std::string aPath, std::string aInterface);

    ~Client() = default;

    Client(Client&& aOther) noexcept;

    Client& operator=(Client&& aOther) noexcept;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /**
     * @brief Synchronously call `aMethod` on the remote service.
     *
     * @tparam Ret         expected return type (default void)
     * @tparam TimeoutUsec optional timeout in microseconds (0 = default)
     * @param aMethod      method name to invoke
     * @param aArgs        call arguments
     * @return Reply<Ret> carrying the parsed return value or an error
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs) {
        if (mLooper->isOwnerThread()) {
            return mAsyncPtr->callSync<Ret, TimeoutUsec>(
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

        PendingReply<Ret> pend = future.get();
        pend.wait();
        return pend.reply();
    }

    /**
     * @brief Asynchronously call `aMethod` and return a #PendingReply handle.
     *
     * @tparam Ret          expected return type (default void)
     * @tparam TimeoutUsec  optional timeout in microseconds (0 = default)
     * @param aMethod       method name to invoke
     * @param aArgs         call arguments
     * @return PendingReply<Ret> handle for the in-flight call
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args,
        std::enable_if_t<!CallbackLikeFirstArg<Ret, Args...>::value, int> = 0>
    [[nodiscard]] PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs) {
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

    /**
     * @brief Asynchronously call `aMethod` with a callback on completion.
     *
     * The callback must be callable as `void(Reply<Ret>)`.
     *
     * @tparam Ret         expected return type (default void)
     * @tparam TimeoutUsec optional timeout in microseconds (0 = default)
     * @param aMethod      method name to invoke
     * @param aCallback    completion callback
     * @param aArgs        call arguments
     * @return Status indicating whether the call was dispatched
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args,
        std::enable_if_t<std::is_invocable_r_v<void, Callback, Reply<Ret>>, int> = 0>
    [[nodiscard]] Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
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

    /**
     * @brief Subscribe to a signal and invoke `aCallback` on arrival.
     *
     * @param aSignal   signal name to listen for
     * @param aCallback callback invoked on arrival
     * @return Status of the subscription
     */
    template<typename Callback>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Callback&& aCallback) {
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

    /**
     * @brief Subscribe to a signal dispatched to a member function of `aCls`.
     *
     * @param aSignal signal name to listen for
     * @param aCls    receiver object
     * @param aFunc   member function invoked on arrival
     * @return Status of the subscription
     */
    template<typename Cls, typename Ret, typename... Args>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
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

    /**
     * @brief Fetch a remote property's value via `Properties.Get`.
     *
     * @tparam Ret  expected property type
     * @param aProp property name
     * @return Reply<Ret> carrying the property value or an error
     */
    template<typename Ret>
    [[nodiscard]] Reply<Ret> getProperty(std::string_view aProp) {
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

    /**
     * @brief Set a remote property's value via `Properties.Set`.
     *
     * @param aProp  property name
     * @param aValue new value to write
     * @return Status of the write
     */
    template<typename T>
    [[nodiscard]] Status setProperty(std::string_view aProp, const T& aValue) {
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

    /**
     * @brief Subscribe to changes of a remote property (`PropertiesChanged`).
     *
     * @param aProp     property name to watch
     * @param aCallback callback invoked with the new value
     * @return Status of the subscription
     */
    template<typename Callback>
    [[nodiscard]] Status onPropertyChanged(std::string_view aProp, Callback&& aCallback) {
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
    static Session createSession(SessionType aType, std::string_view aService);

    static std::shared_ptr<AsyncPool> getAsyncPool(
        SessionType aType, std::string_view aService = "");

    std::shared_ptr<AsyncPool> mAsyncPool { nullptr };
    Session* mAsyncPtr { nullptr };
    Looper* mLooper { nullptr };
    SessionType mType { SessionType::INVALID };
    ServerInfo mInfo;
};

}

#endif