
#ifndef DBUSXX_DBUS_SESSION_HPP
#define DBUSXX_DBUS_SESSION_HPP

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "private/method/Method.hpp"
#include "private/method/Reconnect.hpp"
#include "private/session/SessionPrivate.hpp"
#include "Message.hpp"
#include "MetaObject.hpp"
#include "PendingReply.hpp"
#include "Reply.hpp"
#include "Status.hpp"


namespace Dbusxx {
/**
 * @brief A D-Bus session (connection) bound to a specific bus.
 *
 * `Session` is the core type of the library: it manages a single
 * D-Bus connection, lets you register methods/signals/properties
 * (via `RegisterBuilder`), make synchronous and asynchronous remote
 * calls, and emit/listen to signals. Sessions are created through the
 * static factory methods (`systemSession()`, `userSession()`,
 * `peerSession()`, `createSession()`).
 */
class Session {
    friend class Looper;
    using PendingRepsV = std::vector<std::shared_ptr<void>>;

    template<typename Ret, typename... Args>
    struct CallbackLikeFirstArg {
        static constexpr bool value = false;
    };

    template<typename Ret, typename First, typename... Rest>
    struct CallbackLikeFirstArg<Ret, First, Rest...> {
        static constexpr bool value =
            std::is_invocable_r_v<void, First, Reply<Ret>>;
    };

public:
    /**
     * @brief Chainable builder that batches registrations for one (path, interface).
     *
     * Obtain one via `Session::registerBuilder()`, add methods, signals and
     * properties, then call `commit()` to publish them as a single vtable.
     */
    class RegisterBuilder {
    public:
        RegisterBuilder(RegisterBuilder&&) noexcept = default;
        RegisterBuilder& operator=(RegisterBuilder&&) noexcept = default;
        RegisterBuilder(const RegisterBuilder&) = delete;
        RegisterBuilder& operator=(const RegisterBuilder&) = delete;

        /**
         * @brief Register a method backed by an arbitrary callable.
         *
         * The callable may be a lambda, function object or `std::function`.
         *
         * @param aName method name exposed on the bus
         * @param aFunc callable implementing the method
         * @return *this for chaining
         */
        template<typename Func>
        RegisterBuilder& addMethod(std::string_view aName, Func aFunc) {
            using wrapper = Method::MethodWrapper<Func>;
            auto data = std::make_shared<wrapper>(mSession, aFunc);
            void* dataPtr = data.get();
            mSession->addMethodEntry(mKey, aName,
                wrapper::input(), wrapper::output(),
                &wrapper::onCall, dataPtr, std::move(data));
            return *this;
        }

        /**
         * @brief Register a method backed by a member function of `aCls`.
         *
         * @param aName method name exposed on the bus
         * @param aCls  receiver object
         * @param aFunc member function implementing the method
         * @return *this for chaining
         */
        template<typename Cls, typename Ret, typename ...Args>
        RegisterBuilder& addMethod(std::string_view aName, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
            return addMethod(aName,
                [aCls, aFunc] (Args... aArgs) -> Ret {
                    return (aCls->*aFunc)(aArgs...);
                }
            );
        }

        /**
         * @brief Register a signal with the given argument types.
         *
         * @tparam Args signal argument types
         * @param aName signal name exposed on the bus
         * @return *this for chaining
         */
        template<typename... Args>
        RegisterBuilder& addSignal(std::string_view aName) {
            mSession->addSignalEntry(mKey, aName,
                Method::getArgsString<Args...>());
            return *this;
        }

        /**
         * @brief Register a property holding the initial value `aValue`.
         *
         * The wrapper owns its own copy of the value.
         *
         * @tparam T       property value type
         * @param aName    property name exposed on the bus
         * @param aValue   initial value
         * @param writable whether remote clients may write to it (default true)
         * @return *this for chaining
         */
        template<typename T>
        RegisterBuilder& addProperty(std::string_view aName, T aValue, bool writable = true) {
            using wrapper = Method::PropertyWrapper<T>;
            auto data = std::make_shared<wrapper>(
                mSession, aName.data(), mPath, mIface, aValue);
            void* dataPtr = data.get();
            mSession->addPropertyEntry(mKey, aName, wrapper::signature(),
                writable, &wrapper::onGetter, &wrapper::onSetter,
                dataPtr, std::move(data));
            return *this;
        }

        /**
         * @brief Commit all queued registrations for this builder.
         *
         * Publishes the accumulated vtable on the session; the builder cannot
         * be extended further afterwards.
         *
         * @return Status of the registration
         */
        [[nodiscard]] Status commit() {
            return mSession->commitBuilder(mKey);
        }

    private:
        friend class Session;

        RegisterBuilder(Private::SessionPrivate* aSession,
            std::string aKey, std::string aPath, std::string aIface)
            : mSession(aSession)
            , mKey(std::move(aKey))
            , mPath(std::move(aPath))
            , mIface(std::move(aIface)) {}

        Private::SessionPrivate* mSession;
        std::string mKey;
        std::string mPath;
        std::string mIface;
    };

    ~Session() = default;

    Session(const Session& aOther) = default;
    Session& operator=(const Session& aOther) = default;

    Session(Session&& aOther) noexcept = default;
    Session& operator=(Session&& aOther) noexcept = default;

    //! @brief Create a session on the system bus.
    static Session systemSession();

    /**
     * @brief Create a session on the system bus, requesting the given service name.
     *
     * @param aServiceName service name to request
     * @return the created session
     */
    static Session systemSession(std::string_view aServiceName);

    //! @brief Create a session on the user/session bus.
    static Session userSession();

    /**
     * @brief Create a session on the user/session bus with the given service name.
     *
     * @param aServiceName service name to request
     * @return the created session
     */
    static Session userSession(std::string_view aServiceName);

    /**
     * @brief Create a peer-to-peer session over a socket (no bus daemon).
     *
     * @param aServiceName socket address / service name
     * @param aIsServer    whether this end accepts the connection (default false)
     * @return the created session
     */
    static Session peerSession(std::string_view aServiceName, bool aIsServer = false);

    /**
     * @brief Generic factory: create a session of the given type.
     *
     * @param aType         session type (system/user/peer)
     * @param aServiceName  service name or socket address (default empty)
     * @param aIsServer     whether this end is a peer server (default false)
     * @return the created session
     */
    static Session createSession(SessionType aType = SessionType::USER,
        std::string_view aServiceName = "", bool aIsServer = false);

    //! @brief Return the type of bus this session is connected to.
    [[nodiscard]] inline SessionType type() const {
        return mPrivate->type();
    }

    //! @brief Return the well-known service name this session requested at creation.
    [[nodiscard]] inline std::string serviceName() const {
        return mPrivate->serviceName();
    }

    //! @brief Return the underlying file descriptor of the connection.
    [[nodiscard]] inline int getFd() const {
        return mPrivate->getFd();
    }

    /**
     * @brief Process one batch of pending events.
     *
     * @return the sd-bus process return code
     */
    int process();

    /**
     * @brief Block for events until `aTimeoutUsec` elapses.
     *
     * @param aTimeoutUsec timeout in microseconds, passed straight to
     *                     sd_bus_wait (default UINT64_MAX = wait forever)
     * @return the sd-bus wait return code
     */
    int wait(uint64_t aTimeoutUsec = UINT64_MAX);

    //! @brief Flush buffered outgoing messages to the bus.
    void flush();

    /**
     * @brief Create a chainable builder for the given (path, interface).
     *
     * @param aPath  object path
     * @param aIface interface name
     * @return a builder to register methods/signals/properties
     */
    RegisterBuilder registerBuilder(std::string_view aPath, std::string_view aIface);

    /**
     * @brief Register a method backed by an arbitrary callable.
     *
     * @param aPath     object path
     * @param aIface    interface name
     * @param aFuncName method name
     * @param aFunc     callable implementing the method
     * @return Status of the registration
     */
    template<typename Func>
    [[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
        std::string_view aFuncName, Func&& aFunc) {
        return registerBuilder(aPath, aIface)
            .addMethod(aFuncName, std::forward<Func>(aFunc))
            .commit();
    }

    /**
     * @brief Register a method backed by a member function of `aCls`.
     *
     * @param aPath     object path
     * @param aIface    interface name
     * @param aFuncName method name
     * @param aCls      receiver object
     * @param aFunc     member function implementing the method
     * @return Status of the registration
     */
    template<typename Cls, typename Ret, typename... Args>
    [[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
        std::string_view aFuncName, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return registerMethod(
            aPath, aIface, aFuncName,
            [aCls, aFunc] (Args... aArgs) -> Ret {
                return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
            }
        );
    }

    /**
     * @brief Register a signal of the given argument types.
     *
     * @tparam Args       signal argument types
     * @param aPath       object path
     * @param aIface      interface name
     * @param aSignalName signal name
     * @return Status of the registration
     */
    template<typename... Args>
    [[nodiscard]] Status registerSignal(std::string_view aPath, std::string aIface,
            std::string_view aSignalName) {
        return registerBuilder(aPath, aIface)
            .addSignal(aSignalName)
            .commit();
    }

    /**
     * @brief Register all reflection-annotated members of `aObj`.
     *
     * Consumes the metadata collected by the `MetaObject` macros
     * (`DBUSXX_METHOD`, `DBUSXX_SIGNAL`, `DBUSXX_PROPERTY_*`, ...).
     *
     * @param aPath  object path
     * @param aIface interface name
     * @param aObj   object exposing the annotated members
     * @return Status of the registration
     */
    template<typename T>
    [[nodiscard]] Status registerObject(std::string_view aPath, std::string aIface, T* aObj) {
        auto builder = registerBuilder(aPath, aIface);
        for (auto& entry : MetaObject<T>::registry()) {
            entry.registerFn(&builder, aObj);
        }
        return builder.commit();
    }

    /**
     * @brief Synchronously call a remote method and return a typed reply.
     *
     * Blocks until the reply arrives.
     *
     * @tparam Ret         expected return type (default void)
     * @tparam TimeoutUsec optional timeout in microseconds (0 = default)
     * @param aService     remote service name
     * @param aPath        remote object path
     * @param aIface       remote interface name
     * @param aMethod      method name to invoke
     * @param aArgs        call arguments
     * @return Reply<Ret> carrying the parsed return value or an error
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Reply<Ret>(
            Method::callSync<>(
                mPrivate.get(), TimeoutUsec, aService, aPath, aIface, aMethod, aArgs...
            ));
    }

    /**
     * @brief Asynchronously call a remote method and return a handle.
     *
     * @tparam Ret         expected return type (default void)
     * @tparam TimeoutUsec optional timeout in microseconds (0 = default)
     * @param aService     remote service name
     * @param aPath        remote object path
     * @param aIface       remote interface name
     * @param aMethod      method name to invoke
     * @param aArgs        call arguments
     * @return PendingReply<Ret> handle for the in-flight call
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args,
        std::enable_if_t<!CallbackLikeFirstArg<Ret, Args...>::value, int> = 0>
    [[nodiscard]] PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        static_assert((isValidArgs<Args>() && ...),
            "callAsync: arguments must be valid D-Bus types. If you meant a callback, "
            "it must be callable as void(Reply<Ret>), e.g. "
            "callAsync<int>(..., [](Reply<int> r){}, args...)");

        return PendingReply<Ret>(
            Method::callAsync<>(
                mPrivate.get(), TimeoutUsec, aService, aPath, aIface, aMethod, aArgs...
        ));
    }

    /**
     * @brief Asynchronously call a remote method with a completion callback.
     *
     * The callback must be callable as `void(Reply<Ret>)`.
     *
     * @tparam Ret         expected return type (default void)
     * @tparam TimeoutUsec optional timeout in microseconds (0 = default)
     * @param aService     remote service name
     * @param aPath        remote object path
     * @param aIface       remote interface name
     * @param aMethod      method name to invoke
     * @param aCallback    completion callback
     * @param aArgs        call arguments
     * @return Status indicating whether the call was dispatched
     */
    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args,
        std::enable_if_t<std::is_invocable_r_v<void, Callback, Reply<Ret>>, int> = 0>
    [[nodiscard]] Status callAsync(std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
        using Call = std::function<void(Reply<Ret>)>;
        auto rep = std::make_shared<PendingReply<Ret>>(
            Method::callAsync<>(mPrivate.get(), TimeoutUsec,
                aService, aPath, aIface, aMethod, aArgs...)
        );

        mRepsPtr->push_back(rep);
        rep->setCallback(
            [RepsPtr = mRepsPtr, cb = Call(std::forward<Callback>(aCallback)),
                key = std::weak_ptr<void>(rep)] (Reply<Ret> aRep) {
                auto locked = key.lock();
                if (!locked) {
                    return;
                }
                //! Use RAII to ensure release old rep
                struct Clear {
                    std::vector<std::shared_ptr<void>>& reps;
                    std::shared_ptr<void> entry;
                    ~Clear() {
                        reps.erase(std::find(reps.begin(), reps.end(), entry));
                    }
                } clear{*RepsPtr, locked};

                cb(aRep);
            }
        );

        return rep->getStatus();
    }

    /**
     * @brief Subscribe to a signal and invoke `aCallback` on arrival.
     *
     * @param aSender   sender unique name to match (empty for any)
     * @param aPath     object path to match
     * @param aIface    interface name to match
     * @param aSignal   signal name to listen for
     * @param aCallback callback invoked on arrival
     * @return Status of the subscription
     */
    template<typename Callback>
    [[nodiscard]] Status listenSignal(std::string_view aSender,
        std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Callback&& aCallback) {
        return Method::listenSignal(
            mPrivate.get(), aSender, aPath, aIface, aSignal,
            std::forward<Callback>(aCallback)
        );
    }

    /**
     * @brief Subscribe to a signal dispatched to a member function of `aCls`.
     *
     * @param aSender sender unique name to match (empty for any)
     * @param aPath   object path to match
     * @param aIface  interface name to match
     * @param aSignal signal name to listen for
     * @param aCls    receiver object
     * @param aFunc   member function invoked on arrival
     * @return Status of the subscription
     */
    template<typename Cls, typename Ret, typename... Args>
    [[nodiscard]] Status listenSignal(std::string_view aSender,
        std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return Method::listenSignal(
            mPrivate.get(), aSender, aPath, aIface, aSignal,
            [aCls, aFunc] (Args... aArgs) -> Ret {
                return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
            }
        );
    }

    /**
     * @brief Emit a signal with the given arguments.
     *
     * @param aPath   object path
     * @param aIface  interface name
     * @param aSignal signal name to emit
     * @param aArgs   signal arguments
     * @return Status of the emission
     */
    template<typename... Args>
    [[nodiscard]] Status emitSignal(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, const Args&... aArgs) {
        return Method::emitSignal(
            mPrivate.get(), aPath, aIface, aSignal, aArgs...
        );
    }

    /**
     * @brief Read the current value of a locally registered property.
     *
     * @tparam T     property value type
     * @param aPath  object path
     * @param aIface interface name
     * @param aName  property name
     * @param aValue out-parameter receiving the value
     * @return Status of the read
     */
    template<typename T>
    [[nodiscard]] Status getLocalProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        aValue = (*p).get();
        return Status(StatusCode::SUCCESS);
    }

    /**
     * @brief Write a new value to a locally registered property.
     *
     * @tparam T     property value type
     * @param aPath  object path
     * @param aIface interface name
     * @param aName  property name
     * @param aValue new value to write
     * @return Status of the write
     */
    template<typename T>
    [[nodiscard]] Status setLocalProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, const T& aValue) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).set(aValue);
        return Status(StatusCode::SUCCESS);
    }

    /**
     * @brief Register a callback fired when a local property changes.
     *
     * @tparam T        property value type
     * @param aPath     object path
     * @param aIface    interface name
     * @param aName     property name
     * @param aCallback callback invoked with the new value
     * @return Status of the registration
     */
    template<typename T>
    [[nodiscard]] Status onLocalPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).onChanged(std::forward<std::function<void(const T&)>>(aCallback));
        return Status(StatusCode::SUCCESS);
    }

    /**
     * @brief Fetch a remote property's value via `Properties.Get`.
     *
     * @tparam Ret     expected property type
     * @param aService remote service name
     * @param aPath    remote object path
     * @param aIface   remote interface name
     * @param aProp    property name
     * @return Reply<Ret> carrying the property value or an error
     */
    template<typename Ret>
    [[nodiscard]] Reply<Ret> getRemoteProperty(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aProp) {
        return Reply<Ret>(
            Method::getRemoteProperty(
                mPrivate.get(), aService, aPath, aIface, aProp
        ));
    }

    /**
     * @brief Set a remote property's value via `Properties.Set`.
     *
     * @tparam T       property value type
     * @param aService remote service name
     * @param aPath    remote object path
     * @param aIface   remote interface name
     * @param aProp    property name
     * @param aValue   new value to write
     * @return Status of the write
     */
    template<typename T>
    [[nodiscard]] Status setRemoteProperty(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aProp, const T& aValue) {
        return Method::setRemoteProperty(
            mPrivate.get(), aService, aPath, aIface, aProp, aValue);
    }

    /**
     * @brief Subscribe to changes of a remote property (`PropertiesChanged`).
     *
     * @param aService  remote service name
     * @param aPath     remote object path
     * @param aIface    remote interface name
     * @param aProp     property name to watch
     * @param aCallback callback invoked with the new value
     * @return Status of the subscription
     */
    template<typename Callback>
    [[nodiscard]] Status onRemotePropertyChanged(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aProp, Callback&& aCallback) {
        return Method::onRemotePropertyChanged(
            mPrivate.get(), aService, aPath, aIface, aProp, std::forward<Callback>(aCallback)
        );
    }

private:
    explicit Session(SessionType aType,
        std::string_view aServiceName = "", bool aIsServer = false);

    template<typename T>
    Method::PropertyWrapper<T>* getPropPrivate(std::string_view aPath,
        std::string_view aIface, std::string_view aName) {
        auto objIter = mPrivate->objects().find(
            Private::SessionPrivate::ObjectInfo::makeKey(aPath, aIface));
        if (objIter == mPrivate->objects().end()) {
            return nullptr;
        }

        auto& properties = objIter->second.properties;
        auto propIter = properties.find(aName.data());
        if (propIter == properties.end()) {
            return nullptr;
        }

        auto p = static_cast<Method::PropertyWrapper<T>*>(propIter->second.data.get());
        if (p->type != typeid(T).name()) {
            return nullptr;
        }
        
        return p;
    }

    std::shared_ptr<Private::SessionPrivate> mPrivate { nullptr };
    std::shared_ptr<PendingRepsV> mRepsPtr { nullptr };
};
}
#endif
