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
template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    Server() = delete;

    explicit Server(SessionType aType, std::string_view aServiceName)
        : mSession(Session::createSession(aType, aServiceName, true))
        , mLooper(mSession) {}

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

    void run() {
        mLooper.onReady([this]() -> Status {
            init();
            return mStatus;
        });
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
    Status emit(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Args&&... aArgs) {
        if (mLooper.isOwnerThread()) {
            return mSession.emitSignal(aPath, aIface, aSignal, std::forward<Args>(aArgs)...);
        }

        auto argsTuple = std::make_tuple(std::forward<Args>(aArgs)...);
        mLooper.post(
            [this, path = std::string(aPath),
             iface = std::string(aIface),
             signal = std::string(aSignal),
             args = std::move(argsTuple)] () mutable -> void {
                std::apply(
                    [&](auto&&... aUnpacked) {
                        mSession.emitSignal(path, iface, signal, aUnpacked...);
                    }, std::move(args)
                );
            }
        );

        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status getProperty(std::string_view aPath, std::string_view aIface,
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

    template<typename T>
    Status setProperty(std::string_view aPath, std::string_view aIface,
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

    template<typename T>
    Status onPropertyChanged(std::string_view aPath, std::string_view aIface,
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

    [[nodiscard]] inline Status status() const {
        return mStatus.isError() ? mStatus : mLooper.status();
    }

    [[nodiscard]] inline SessionType type() const {
        return mSession.type();
    }

protected:
    Session& session() {
        return mSession;
    }

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