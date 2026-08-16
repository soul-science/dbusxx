
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <memory>
#include <vector>

#include "private/method/Method.hpp"
#include "private/method/Reconnect.hpp"
#include "private/session/SessionPrivate.hpp"
#include "Message.hpp"
#include "MetaObject.hpp"
#include "PendingReply.hpp"
#include "Reply.hpp"
#include "Status.hpp"


namespace SSDbus {
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
    class RegisterBuilder {
    public:
        RegisterBuilder(RegisterBuilder&&) noexcept = default;
        RegisterBuilder& operator=(RegisterBuilder&&) noexcept = default;
        RegisterBuilder(const RegisterBuilder&) = delete;
        RegisterBuilder& operator=(const RegisterBuilder&) = delete;

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

        template<typename Cls, typename Ret, typename ...Args>
        RegisterBuilder& addMethod(std::string_view aName, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
            return addMethod(aName,
                [aCls, aFunc] (Args... aArgs) -> Ret {
                    return (aCls->*aFunc)(aArgs...);
                }
            );
        }

        template<typename... Args>
        RegisterBuilder& addSignal(std::string_view aName) {
            mSession->addSignalEntry(mKey, aName,
                Method::getArgsString<Args...>());
            return *this;
        }

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

    Status commit() {
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

    static Session systemSession();

    static Session systemSession(std::string_view aServiceName);

    static Session userSession();

    static Session userSession(std::string_view aServiceName);

    static Session peerSession(std::string_view aServiceName, bool aIsServer = false);

    static Session createSession(SessionType aType = SessionType::USER,
        std::string_view aServiceName = "", bool aIsServer = false);

    inline SessionType type() const {
        return mPrivate->type();
    }

    inline std::string serviceName() const {
        return mPrivate->serviceName();
    }

    inline int getFd() const {
        return mPrivate->getFd();
    }

    int process();

    int wait(uint64_t aTimeoutMs = UINT64_MAX);

    void flush();

    RegisterBuilder registerBuilder(std::string_view aPath, std::string_view aIface);

    template<typename Func>
    Status registerMethod(std::string_view aPath, std::string aIface,
        std::string_view aFuncName, Func&& aFunc) {
        return registerBuilder(aPath, aIface)
            .addMethod(aFuncName, std::forward<Func>(aFunc))
            .commit();
    }

    template<typename Cls, typename Ret, typename... Args>
    Status registerMethod(std::string_view aPath, std::string aIface,
        std::string_view aFuncName, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return registerMethod(
            aPath, aIface, aFuncName,
            [aCls, aFunc] (Args... aArgs) -> Ret {
                return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
            }
        );
    }

    template<typename... Args>
    Status registerSignal(std::string_view aPath, std::string aIface,
            std::string_view aSignalName) {
        return registerBuilder(aPath, aIface)
            .addSignal(aSignalName)
            .commit();
    }

    template<typename T>
    Status registerObject(std::string_view aPath, std::string aIface, T* aObj) {
        auto builder = registerBuilder(aPath, aIface);
        for (auto& entry : MetaObject<T>::registry()) {
            entry.registerFn(&builder, aObj);
        }
        return builder.commit();
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Reply<Ret>(
            Method::callSync<>(
                mPrivate.get(), TimeoutUsec, aService, aPath, aIface, aMethod, aArgs...
            ));
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args,
        std::enable_if_t<!CallbackLikeFirstArg<Ret, Args...>::value, int> = 0>
    PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
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

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args,
        std::enable_if_t<std::is_invocable_r_v<void, Callback, Reply<Ret>>, int> = 0>
    Status callAsync(std::string_view aService, std::string_view aPath, std::string_view aIface,
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

    template<typename Callback>
    Status listenSignal(std::string_view aSender,
        std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Callback&& aCallback) {
        return Method::listenSignal(
            mPrivate.get(), aSender, aPath, aIface, aSignal,
            std::forward<Callback>(aCallback)
        );
    }

    template<typename Cls, typename Ret, typename... Args>
    Status listenSignal(std::string_view aSender,
        std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return Method::listenSignal(
            mPrivate.get(), aSender, aPath, aIface, aSignal,
            [aCls, aFunc] (Args... aArgs) -> Ret {
                return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
            }
        );
    }

    template<typename... Args>
    Status emitSignal(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, const Args&... aArgs) {
        return Method::emitSignal(
            mPrivate.get(), aPath, aIface, aSignal, aArgs...
        );
    }

    template<typename T>
    Status getLocalProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        aValue = (*p).get();
        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status setLocalProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, const T& aValue) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).set(aValue);
        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status onLocalPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback) {
        auto p = getPropPrivate<T>(aPath, aIface, aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).onChanged(std::forward<std::function<void(const T&)>>(aCallback));
        return Status(StatusCode::SUCCESS);
    }

    template<typename Ret>
    Reply<Ret> getRemoteProperty(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aProp) {
        return Reply<Ret>(
            Method::getRemoteProperty(
                mPrivate.get(), aService, aPath, aIface, aProp
        ));
    }

    template<typename T>
    Status setRemoteProperty(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aProp, const T& aValue) {
        return Method::setRemoteProperty(
            mPrivate.get(), aService, aPath, aIface, aProp, aValue);
    }

    template<typename Callback>
    Status onRemotePropertyChanged(std::string_view aService, std::string_view aPath,
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
