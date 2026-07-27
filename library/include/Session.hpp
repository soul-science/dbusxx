
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <memory>
#include <vector>

#include "Message.hpp"
#include "Reply.hpp"
#include "PendingReply.hpp"
#include "Status.hpp"
#include "MetaObject.hpp"

#include "adaptor/RawBusSharePtr.hpp"
#include "session/SessionPrivate.hpp"
#include "session/VTableRegistrar.hpp"
#include "method/Method.hpp"

#include <iostream>

namespace SSDbus {

class Session {
    friend class Looper;
    using PendingRepsV = std::vector<std::shared_ptr<void>>;

public:
    struct RegisterBuilder {
        Private::SessionPrivate* session;
        Private::VTableRegistrar reg;
        Private::SessionPrivate::MethodMap mMethods;
        Private::SessionPrivate::SignalMap mSignals;
        Private::SessionPrivate::PropertyMap mProperties;

        template<typename Func>
        RegisterBuilder& addMethod(std::string_view aName, Func aFunc) {
            using wrapper = Method::MethodWrapper<Func>;
            std::string input = wrapper::input();
            std::string output = wrapper::output();
            std::string regName = aName.data() + std::string("_") + input;
            std::cout << "addMethod: " << regName << std::endl;
            auto data = std::make_shared<wrapper>(session, aFunc);
            mMethods[aName.data()] = {
                std::move(data), nullptr
            };
            reg.addMethod(aName, std::move(input), std::move(output),
                &wrapper::onCall, mMethods[aName.data()].data.get());

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
            std::string input = Method::getArgsString<Args...>();
            std::string regName = aName.data() + std::string("_") + input;
            std::cout << "addSignal: " << regName << std::endl;
            mSignals[aName.data()] = {nullptr};
            reg.addSiganl(aName.data(), std::move(input));

            return *this;
        }

        template<typename T>
        RegisterBuilder& addProperty(std::string_view aName, T aValue) {
            using wrapper = Method::PropertyWrapper<T>;
            std::string sig = wrapper::signature();
            
            auto data = std::make_shared<wrapper>(
                session, aName.data(), aValue);
            mProperties[aName.data()] = {std::move(data), nullptr};
            
            reg.addProperty(aName, sig,
                &wrapper::onGetter, &wrapper::onSetter,
                mProperties[aName.data()].data.get());

            return *this;
        }

        Status commit() {
            for (auto& [name, _] : mMethods) {
                if (session->methods().count(name)) {
                    std::cout << "Method <" << name << "> already registered" << std::endl;
                    return Status(StatusCode::NAME_EXISTS);
                }
            }

            for (auto& [name, _] : mSignals) {
                if (session->signals().count(name)) {
                    std::cout << "Signal <" << name << "> already registered" << std::endl;
                    return Status(StatusCode::NAME_EXISTS);
                }
            }

            for (auto& [name, _] : mProperties) {
                if (session->properties().count(name)) {
                    std::cout << "Property <" << name << "> already registered" << std::endl;
                    return Status(StatusCode::NAME_EXISTS);
                }
            }

            std::vector<std::unique_ptr<Private::VTableContext>> ctxs;
            auto st = reg.commit(ctxs);
            if (st.isError()) {
                return st;
            }

            size_t i = 0;
            for (auto& [k, v] : mMethods) {
                v.context = std::move(ctxs[i++]);
                session->methods()[k] = std::move(v);
            }

            for (auto& [k, v] : mSignals) {
                v.context = std::move(ctxs[i++]);
                session->signals()[k] = std::move(v);
            }

            for (auto& [k, v] : mProperties) {
                v.context = std::move(ctxs[i++]);
                session->properties()[k] = std::move(v);
            }

            return Status(StatusCode::SUCCESS);
        }
    };

    explicit Session(bool aIsSystem = false)
        : mPrivate(std::make_shared<Private::SessionPrivate>(aIsSystem))
        , mRepsPtr(std::make_shared<PendingRepsV>()) {}

    ~Session() = default;

    Session(const Session& aOther) = default;
    Session& operator=(const Session& aOther) = default;

    Session(Session&& aOther) noexcept = default;
    Session& operator=(Session&& aOther) noexcept = default;

    static Session systemBus() {
        return Session(true);
    }

    static Session sessionBus() {
        return Session(false);
    }

    Status setInfo(ServiceInfo aInfo) {
        return mPrivate->setInfo(aInfo);
    }

    int process() {
        return mPrivate->process();
    }

    int wait(uint64_t aTimeoutMs = UINT64_MAX) {
        return mPrivate->wait(aTimeoutMs);
    }

    int getFd() const {
        return mPrivate->getFd();
    }

    void flush() {
        mPrivate->flush();
    }

    auto registerBuilder() {
        return RegisterBuilder {
            mPrivate.get(),
            Private::VTableRegistrar(
                mPrivate.get()->rawBus(), mPrivate->info().path,
                mPrivate->info().interface)
        };
    }

    template<typename Func>
    Status registerMethod(std::string_view aFuncName, Func aFunc) {
        return Method::registerSingleMethod(mPrivate.get(), aFuncName, aFunc);
    }

    template<typename Cls, typename Ret, typename... Args>
    Status registerMethod(std::string_view aFuncName,
        Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
        return Method::registerSingleMethod(
            mPrivate.get(), aFuncName,
            [aCls, aFunc] (Args... aArgs) -> Ret {
                return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
            }
        );
    }

    template<typename... Args>
    Status registerSignal(std::string_view aSignalName) {
        return Method::registerSingleSignal<Args...>(mPrivate.get(), aSignalName);
    }

    template<typename T>
    Status registerObject(T* aObj) {
        auto builder = registerBuilder();
        for (auto& entry : MetaObject<T>::registry()) {
            switch (static_cast<typename MetaObject<T>::EntryType>(entry.type)) {
                case MetaObject<T>::EntryType::SignalListen: {
                    entry.registerFn(this, aObj);
                    break;
                }
                default: {
                    entry.registerFn(&builder, aObj);
                }
            }
        }
        return builder.commit();
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return Reply<Ret>(
            Method::callSync<Ret, Args...>(
                mPrivate.get(), TimeoutUsec, aService, aPath, aIface, aMethod, aArgs...
            ));
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return PendingReply<Ret>(
            Method::callAsync<Ret, Args...>(
                mPrivate.get(), TimeoutUsec, aService, aPath, aIface, aMethod, aArgs...
        ));
    }

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
    Status callAsync(std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aMethod, Callback&& aCallback, const Args&... aArgs) {
        static_assert(std::is_invocable_r_v<void, Callback, Reply<Ret>>,
            "callAsync callback must be callable as: void(Reply<Ret>)");
        using Call = std::function<void(Reply<Ret>)>;
        auto rep = std::make_shared<PendingReply<Ret>>(
            Method::callAsync<Ret, Args...>(mPrivate.get(), TimeoutUsec,
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
    Status emitSignal(std::string_view aSignal, const Args&... aArgs) {
        ServiceInfo info = mPrivate->info();
        return Method::emitSignal(
            mPrivate.get(), info.path, info.interface, aSignal, aArgs...
        );
    }

    template<typename T>
    Status getLocalProperty(std::string_view aName, T& aValue) {
        auto p = getPropPrivate<T>(aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        aValue = (*p).get();
        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status setLocalProperty(std::string_view aName, T aValue) {
        auto p = getPropPrivate<T>(aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).set(aValue);
        return Status(StatusCode::SUCCESS);
    }

    template<typename T>
    Status onLocalPropertyChanged(std::string_view aName, std::function<void(const T&)>&& aCallback) {
        auto p = getPropPrivate<T>(aName);
        if (!p) {
            return Status(StatusCode::INVALID_ARG);
        }

        (*p).onChanged(std::forward<std::function<void(const T&)>>(aCallback));
        return Status(StatusCode::SUCCESS);
    }

private:
    template<typename T>
    Method::PropertyWrapper<T>* getPropPrivate(std::string_view aName) {
        auto it = mPrivate->properties().find(aName.data());
        if (it == mPrivate->properties().end()) {
            return nullptr;
        }

        auto p = static_cast<Method::PropertyWrapper<T>*>(it->second.data.get());
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
