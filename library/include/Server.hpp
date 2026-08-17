#ifndef DBUSXX_DBUS_SERVER_HPP
#define DBUSXX_DBUS_SERVER_HPP

#include <string>
#include <string_view>
#include <future>
#include <tuple>

#include "Looper.hpp"
#include "MetaObject.hpp"
#include "Session.hpp"
#include "Status.hpp"
#include "Utils.hpp"


namespace Dbusxx {
/**
 * @brief One-stop server wrapper bundling a #Session, a #Looper and reflection.
 *
 * `Server<Derived>` (CRTP) creates a session and an event loop. On `run()`
 * it registers the interface annotated on `Derived` with the `DBUSXX_*`
 * macros, then serves it until stopped.
 */
template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    //! @brief A server must always be constructed with a session type and name.
    Server() = delete;

    /**
     * @brief Construct a server of the given session type.
     *
     * @param aType        session type (system/user/peer)
     * @param aServiceName service name to request / socket address
     */
    explicit Server(SessionType aType, std::string_view aServiceName)
        : mSession(Session::createSession(aType, aServiceName, true))
        , mLooper(mSession) {}

    /**
     * @brief Construct a user-session server with the given service name.
     *
     * @param aServiceName service name to request
     */
    explicit Server(std::string_view aServiceName)
        : mSession(Session::createSession(SessionType::USER, aServiceName))
        , mLooper(mSession) {}

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

    //! @brief Register the annotated interface and run the event loop (blocks).
    void run() {
        mLooper.onReady([this]() -> Status {
            init();
            return mStatus;
        });
        mLooper.run();
    }

    //! @brief Stop the loop gracefully (no-op if the server already errored).
    void stop() {
        if (status().isError()) {
            return;
        }

        mLooper.stop();
    }

    //! @brief Stop the loop unconditionally.
    void forceStop() {
        mLooper.stop();
    }

    /**
     * @brief Post a task to be executed on the server's loop thread.
     *
     * @param aTask task to run
     */
    void post(std::function<void()> aTask) {
        mLooper.post(std::move(aTask));
    }

    /**
     * @brief Emit a signal with arguments (thread-safe; may cross threads).
     *
     * @param aPath   object path
     * @param aIface  interface name
     * @param aSignal signal name to emit
     * @param aArgs   signal arguments
     * @return Status of the emission
     */
    template<typename... Args>
    [[nodiscard]] Status emit(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Args&&... aArgs) {
        if (mLooper.isOwnerThread()) {
            return mSession.emitSignal(aPath, aIface, aSignal, std::forward<Args>(aArgs)...);
        }

        auto argsTuple = std::make_tuple(std::forward<Args>(aArgs)...);
        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper.post(
            [this, &promise, path = std::string(aPath),
             iface = std::string(aIface),
             signal = std::string(aSignal),
             args = std::move(argsTuple)] () mutable -> void {
                std::apply(
                    [&](auto&&... aUnpacked) {
                        promise.set_value(
                            mSession.emitSignal(path, iface, signal, aUnpacked...)
                        );
                    }, std::move(args)
                );
            }
        );

        return future.get();
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
    [[nodiscard]] Status getProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue) {
        if (mLooper.isOwnerThread()) {
            return mSession.template getLocalProperty<T>(
                aPath, aIface, aName, aValue);
        }

        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper.post(
            [this, &promise,
             path = std::string(aPath),
             iface = std::string(aIface),
             name = std::string(aName), &aValue] () mutable -> void {
                promise.set_value(
                    mSession.template getLocalProperty<T>(
                        path, iface, name, aValue)
                );
             }
        );

        return future.get();
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
    [[nodiscard]] Status setProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, const T& aValue) {
        if (mLooper.isOwnerThread()) {
            return mSession.template setLocalProperty<T>(
                aPath, aIface, aName, aValue);
        }

        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper.post(
            [this, &promise,
             path = std::string(aPath),
             iface = std::string(aIface),
             name = std::string(aName),
             value = aValue] () mutable -> void {
                promise.set_value(
                    mSession.template setLocalProperty<T>(
                        path, iface, name, value)
                );
             }
        );

        return future.get();
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
    [[nodiscard]] Status onPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback) {
        if (mLooper.isOwnerThread()) {
            return mSession.template onLocalPropertyChanged<T>(
                aPath, aIface, aName,
                std::forward<std::function<void(const T&)>>(aCallback));
        }

        std::promise<Status> promise;
        std::future<Status> future = promise.get_future();
        mLooper.post(
            [this, &promise,
             path = std::string(aPath),
             iface = std::string(aIface),
             name = std::string(aName),
             cb = std::forward<std::function<void(const T&)>>(aCallback)
            ] () mutable -> void {
                promise.set_value(
                    mSession.template onLocalPropertyChanged<T>(
                        path, iface, name,
                        std::forward<std::function<void(const T&)>>(cb))
                );
             }
        );

        return future.get();
    }

    //! @brief Return the server's current status (error takes precedence).
    [[nodiscard]] inline Status status() const {
        return mStatus.isError() ? mStatus : mLooper.status();
    }

    //! @brief Return the session type this server is bound to.
    [[nodiscard]] inline SessionType type() const {
        return mSession.type();
    }

protected:
    //! @brief Access the underlying session (for direct registration).
    Session& session() {
        return mSession;
    }

    //! @brief Access the underlying event loop.
    Looper& looper() {
        return mLooper;
    }

private:
    void init() {
        if (mInited || mStatus.isError()) {
            return;
        }

        auto& entries = Derived::registry();
        using Entry = typename std::decay_t<decltype(entries)>::value_type;

        std::map<
            std::pair<std::string_view, std::string_view>,
            std::vector<Entry>
        > groups;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            groups[{it->path, it->iface}].push_back(*it);
        }

        for (auto& [key, group] : groups) {
            auto builder = mSession.registerBuilder(key.first, key.second);
            for (auto& e : group) {
                e.registerFn(&builder, static_cast<Derived*>(this));
            }

            mStatus = builder.commit();
            if (mStatus.isError()) {
                return;
            }
        }
        mInited = true;
    }

    Session mSession;
    Looper mLooper;
    Status mStatus;

    bool mInited { false };
};
}
#endif