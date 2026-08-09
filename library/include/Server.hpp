#ifndef SSDBUS_DBUS_SERVER_HPP
#define SSDBUS_DBUS_SERVER_HPP

#include <string_view>
#include <tuple>

#include "Looper.hpp"
#include "session/MetaObject.hpp"
#include "Session.hpp"
#include "Status.hpp"
#include "Utils.hpp"

namespace SSDbus {

template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    Server() = delete;

    explicit Server(SessionType aType, std::string_view aServiceName)
        : mSession(Session::createSession(aType, aServiceName))
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
    Status emit(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Args&&... aArgs) {
        if (mLooper.isOwnerThread()) {
            return mSession.emitSignal(aPath, aIface, aSignal, std::forward<Args>(aArgs)...);
        }

        auto argsTuple = std::make_tuple(std::forward<Args>(aArgs)...);
        mLooper.post(
            [session = mSession,
             path = std::string(aPath),
             iface = std::string(aIface),
             signal = std::string(aSignal),
             args = std::move(argsTuple)] () mutable -> void {
                std::apply(
                    [&](auto&&... aUnpacked) {
                        session.emitSignal(path, iface, signal, aUnpacked...);
                    }, std::move(args)
                );
            }
        );

        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status getProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue) {
        return session().
            template getLocalProperty<T>(aPath, aIface, aName, aValue);
    }

    template<typename T>
    Status setProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T aValue) {
        return session().
            template setLocalProperty<T>(aPath, aIface, aName, aValue);
    }

    template<typename T>
    Status onPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback) {
        return session().
            template onLocalPropertyChanged<T>(aPath, aIface, aName,
                std::forward<std::function<void(const T&)>>(aCallback));
    }

    [[nodiscard]] Status status() const {
        return mStatus.isError() ? mStatus : mLooper.status();
    }

    [[nodiscard]] SessionType type() const {
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

#define SSDBUS_PATH(p)                                                                      \
    static inline int _ssdbus_path_##__LINE__ = [] {                                        \
        Self::sPath = p;                                                                    \
        return 0;                                                                           \
    }();

#define SSDBUS_IFACE(i)                                                                     \
    static inline int _ssdbus_iface_##__LINE__ = [] {                                       \
        Self::sIface = i;                                                                   \
        return 0;                                                                           \
    }();

#define SSDBUS_METHOD(method)                                                               \
    static inline int _ssdbus_reg_##method = [] {                                           \
        Self::registry().push_back({                                                        \
            #method,                                                                        \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addMethod(#method, self, &Self::method);                           \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_SIGNAL(signal, ...)                                                          \
    static inline int _ssdbus_reg_##signal = [] {                                           \
        Self::registry().push_back({                                                        \
            #signal,                                                                        \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addSignal<__VA_ARGS__>(#signal);                                   \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_PROPERTY_RO(name, Type, initValue)                                           \
    static inline int _ssdbus_reg_prop_##name = [] {                                        \
        Self::registry().push_back({                                                        \
            #name,                                                                          \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<Type>(#name, initValue, false);                        \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_PROPERTY_RW(name, Type, initValue)                                           \
    static inline int _ssdbus_reg_prop_##name = [] {                                        \
        Self::registry().push_back({                                                        \
            #name,                                                                          \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<Type>(#name, initValue, true);                         \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

}

#endif