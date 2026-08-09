
#ifndef SSDBUS_DBUS_SESSION_HPP
#define SSDBUS_DBUS_SESSION_HPP

#include <memory>
#include <vector>

#include "Message.hpp"
#include "Reply.hpp"
#include "PendingReply.hpp"
#include "Status.hpp"

#include "adaptor/RawBusSharePtr.hpp"
#include "session/MetaObject.hpp"
#include "session/SessionPrivate.hpp"
#include "method/Method.hpp"
#include "method/Reconnect.hpp"

#include <iostream>

namespace SSDbus {

class Session {
    friend class Looper;
    using PendingRepsV = std::vector<std::shared_ptr<void>>;

public:
    struct RegisterBuilder {
        Private::SessionPrivate* session;
        Adaptor::VTableRegistrar reg;
        std::string key;
        Private::SessionPrivate::ObjectInfo info;

        template<typename Func>
        RegisterBuilder& addMethod(std::string_view aName, Func aFunc) {
            using wrapper = Method::MethodWrapper<Func>;
            std::string input = wrapper::input();
            std::string output = wrapper::output();
            std::string regName = aName.data() + std::string("_") + input;
            std::cout << "addMethod: " << regName << std::endl;
            auto data = std::make_shared<wrapper>(session, aFunc);
            void* dataPtr = data.get();

            info.methods[aName.data()] = {
                std::move(data),
                input,
                output,
                &wrapper::onCall
            };

            reg.addMethod(aName, std::move(input), std::move(output),
                &wrapper::onCall, dataPtr);

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
            info.signals[aName.data()] = {input};
            reg.addSiganl(aName, std::move(input));

            return *this;
        }

        template<typename T>
        RegisterBuilder& addProperty(std::string_view aName, T aValue, bool writable = true) {
            using wrapper = Method::PropertyWrapper<T>;
            std::string sig = wrapper::signature();
            
            auto data = std::make_shared<wrapper>(
                session, aName.data(),
                reg.path(), reg.interface(), aValue
            );
            void* dataPtr = data.get();

            info.properties[aName.data()] = {
                std::move(data),
                sig,
                &wrapper::onGetter,
                &wrapper::onSetter,
                writable
            };
            
            reg.addProperty(aName, std::move(sig),
                &wrapper::onGetter, &wrapper::onSetter,
                dataPtr, writable);

            return *this;
        }

        Status commit() {
            auto& objects = session->objects();
            auto it = objects.find(key);
            if (it != objects.end()) {
                for (auto& [name, entry] : it->second.methods) {
                    if (info.methods.count(name)) {
                        continue;
                    }
                    reg.addMethod(name, entry.input, entry.output,
                        entry.callback, entry.data.get());
                }

                for (auto& [name, entry] : it->second.signals) {
                    if (info.signals.count(name)) {
                        continue;
                    }

                    reg.addSiganl(name, entry.input);
                }

                for (auto& [name, entry] : it->second.properties) {
                    if (info.properties.count(name)) {
                        continue;
                    }

                    reg.addProperty(name, entry.signature,
                        entry.getter, entry.setter,
                        entry.data.get(), entry.writable);
                }
            }


            std::unique_ptr<Adaptor::VTableContext> ctx;
            auto st = reg.commit(ctx);
            if (st.isError()) {
                return st;
            }

            if (it != objects.end()) {
                auto& obj = it->second;
                for (auto& [name, entry] : info.methods) {
                    obj.methods[name] = std::move(entry);   // 覆盖旧 entry
                }
                for (auto& [name, entry] : info.signals) {
                    obj.signals[name] = std::move(entry);
                }
                for (auto& [name, entry] : info.properties) {
                    obj.properties[name] = std::move(entry);
                }
                obj.context = std::move(ctx);
            } else {
                info.context = std::move(ctx);
                objects[key] = std::move(info);
            }

            return Status(StatusCode::SUCCESS);
        }
    };

    ~Session() = default;

    Session(const Session& aOther) = default;
    Session& operator=(const Session& aOther) = default;

    Session(Session&& aOther) noexcept = default;
    Session& operator=(Session&& aOther) noexcept = default;

    static Session systemSession() {
        return Session(SessionType::SYSTEM);
    }

    static Session systemSession(std::string_view aServiceName) {
        Session s(SessionType::SYSTEM, aServiceName);
        s.mPrivate->requestNameToDaemon();
        return s;
    }

    static Session userSession() {
        return Session(SessionType::USER);
    }

    static Session userSession(std::string_view aServiceName) {
        Session s(SessionType::USER, aServiceName);
        s.mPrivate->requestNameToDaemon();
        return s;
    }

    static Session peerSession(std::string_view aServiceName) {
        return Session(SessionType::PEER, aServiceName);
    }

    static Session createSession(SessionType aType = SessionType::USER,
        std::string_view aServiceName = "") {
        switch (aType) {
            case SessionType::SYSTEM:
                return aServiceName.empty() ?
                    systemSession() : systemSession(aServiceName);
            case SessionType::PEER:
                return peerSession(aServiceName);
            case SessionType::USER:
            default:
                return aServiceName.empty() ?
                    userSession() : userSession(aServiceName);
        }
    }

    SessionType type() const {
        return mPrivate->type();
    }

    std::string serviceName() const {
        return mPrivate->serviceName();
    }

    Status rebuild() {
        return Method::reconnectSession(mPrivate.get());
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

    auto registerBuilder(std::string_view aPath, std::string_view aIface) {
        return RegisterBuilder {
            mPrivate.get(),
            Adaptor::VTableRegistrar(
                mPrivate.get()->rawBus(), aPath, aIface),
            Private::SessionPrivate::makeKey(aPath, aIface)
        };
    }

    // template<typename Func>
    // Status registerMethod(std::string_view aPath, std::string aIface,
    //     std::string_view aFuncName, Func aFunc) {
    //     return Method::registerSingleMethod(mPrivate.get(), aFuncName, aFunc);
    // }

    // template<typename Cls, typename Ret, typename... Args>
    // Status registerMethod(std::string_view aPath, std::string aIface,
    //     std::string_view aFuncName, Cls* aCls, Ret(Cls::*aFunc)(Args...)) {
    //     return Method::registerSingleMethod(
    //         mPrivate.get(), aFuncName,
    //         [aCls, aFunc] (Args... aArgs) -> Ret {
    //             return (aCls->*aFunc)(std::forward<Args>(aArgs)...);
    //         }
    //     );
    // }

    // template<typename... Args>
    // Status registerSignal(std::string_view aPath, std::string aIface,
    //         std::string_view aSignalName) {
    //     return Method::registerSingleSignal<Args...>(mPrivate.get(), aSignalName);
    // }

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

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod, const Args&... aArgs) {
        return PendingReply<Ret>(
            Method::callAsync<>(
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
        std::string_view aName, T aValue) {
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
    explicit Session(SessionType aType, std::string_view aServiceName = "")
        : mPrivate(std::make_shared<Private::SessionPrivate>(aType, aServiceName))
        , mRepsPtr(std::make_shared<PendingRepsV>()) {}

    template<typename T>
    Method::PropertyWrapper<T>* getPropPrivate(std::string_view aPath,
        std::string_view aIface, std::string_view aName) {
        auto objIter = mPrivate->objects().find(
            Private::SessionPrivate::makeKey(aPath, aIface));
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
