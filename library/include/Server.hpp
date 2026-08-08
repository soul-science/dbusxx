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

    explicit Server(ServiceInfo aInfo, bool aIsSystem = false)
        : mSession(aIsSystem ?
            Session::systemSession(aInfo) : Session::userSession(aInfo))
        , mLooper(mSession) {}

    explicit Server(std::string_view aSocket)
        : mSession(Session::peerSession(aSocket))
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

    template<typename T>
    Status getProperty(std::string_view aName, T& aValue) {
        return session().template getLocalProperty<T>(aName, aValue);
    }

    template<typename T>
    Status setProperty(std::string_view aName, T aValue) {
        return session().template setLocalProperty<T>(aName, aValue);
    }

    template<typename T>
    Status onPropertyChanged(std::string_view aName, 
                            std::function<void(const T&)>&& aCallback) {
        return session().template onLocalPropertyChanged<T>(aName,
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

        mStatus = mSession.registerObject(static_cast<Derived*>(this));
        mInited = !mStatus.isError();
    }

    Session mSession;
    Looper mLooper;
    Status mStatus;

    bool mInited { false };
};

#define SSDBUS_METHOD(method)                                                               \
    static inline int _ssdbus_reg_##method = [] {                                           \
        Self::registry().push_back({                                                        \
            #method,                                                                        \
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
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<Type>(#name, initValue, true);                         \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

}

#endif