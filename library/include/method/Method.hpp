#ifndef SSDBUS_DBUS_METHOD_HPP
#define SSDBUS_DBUS_METHOD_HPP

#include <map>
#include <iostream>

#include "adaptor/RawRemoteError.hpp"
#include "message/MessagePrivate.hpp"
#include "message/SignalHandler.hpp"
#include "message/PropertyHandler.hpp"
#include "session/SessionPrivate.hpp"
#include "session/VTableRegistrar.hpp"
#include "FunctionTrait.hpp"
#include "Status.hpp"

namespace SSDbus {
namespace Method {

// ========== 共享工具 ==========

template<typename Cls, typename Ret, typename... Args>
std::string getArgsString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
    std::string args;
    if constexpr (sizeof...(Args)) {
        (args.append(getSignature<Args>()), ...);
    }

    return args;
}

template<typename Cls, typename Ret, typename... Args>
std::string getReturnString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
    return getSignature<Ret>();
}

template<typename... Args>
std::string getArgsString() {
    std::string args;
    if constexpr (sizeof...(Args)) {
        (args.append(getSignature<Args>()), ...);
    }
    return args;
}

// ========== 服务端 ==========

template<typename Func>
struct MethodWrapper {
    Private::SessionPrivate* session;
    Func func;

    using FuncType = std::decay_t<Func>;
    using traits = Method::FuncTrait<FuncType>;
    using ArgsTuple = typename traits::ArgsTuple;
    using Ret = typename traits::RetType;

    MethodWrapper(Private::SessionPrivate* aSession, Func aFunc)
        : session(aSession)
        , func(aFunc) {}

    static std::string input() {
        auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
            return getArgsString<Args...>();
        };

        return impl(static_cast<typename traits::ArgsTuple*>(nullptr));
    }

    static std::string output() {
        return getSignature<Ret>();
    }

    static int onCall(Adaptor::RawBusMessagePtr aMsg, void* aUsr, Adaptor::RawBusErrorPtr aErr) {
        auto self = static_cast<MethodWrapper*>(aUsr);
        Private::MessagePrivate message(Adaptor::RawMessageSharePtr(aMsg, false));
        Private::MessagePrivate reply = self->session->createReply(message);
        Status st = Status(StatusCode::SUCCESS);
        auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
            if constexpr (traits::argSize) {
                std::tuple<typename ArgTypeAdaptor<std::decay_t<Args>>::type...> tpl;
                st = message.read(tpl);
                if (st.isError()) {
                    Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr);
                    return -1;
                }

                //! Apply function
                if constexpr (std::is_same_v<Ret, void>) {
                    std::apply(self->func, tpl);
                } else {
                    Ret ret = std::apply(self->func, tpl);
                    st = reply.write(ret);
                    if (st.isError()) {
                        Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr);
                        return -1;
                    }
                }
            }
            else {
                if constexpr (std::is_same_v<Ret, void>) {
                    self->func();
                } else {
                    Ret ret = self->func();
                    st = reply.write(ret);
                    if (st.isError()) {
                        Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr);
                        return -1;
                    }
                }
            }

            st = self->session->sendMessage(reply, message.getSender());
            if (st.isError()) {
                Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr);
                return -1;
            }

            return 0;
        };

        return impl(static_cast<typename traits::ArgsTuple*>(nullptr));
    }
};

template<typename Func>
Status registerSingleMethod(
    Private::SessionPrivate* aSession, std::string_view aFuncName, Func aFunc) {
    using wrapper = MethodWrapper<Func>;
    auto data = std::make_shared<wrapper>(
        aSession, aFunc
    );
    auto dataPtr = data.get();

    std::string input = wrapper::input();
    std::string output = wrapper::output();

    std::cout << "input: " << input << ", output:" << output << std::endl;

    if (aSession->methods().count(aFuncName.data())) {
        return Status(StatusCode::NAME_EXISTS);
    }

    aSession->methods()[aFuncName.data()] = {};
    auto& methodInfo = aSession->methods()[aFuncName.data()];

    methodInfo.data = data;

    //! Create vtable
    Private::VTableRegistrar reg(aSession->rawBus(), aSession->info().path, aSession->info().interface);
    reg.addMethod(aFuncName, input, output, &MethodWrapper<Func>::onCall, dataPtr);
    std::vector<std::unique_ptr<Private::VTableContext>> v;
    auto st = reg.commit(v);
    std::cout << "VTableRegistrar code=" << static_cast<int>(st.code())
        << ", message=" << st.message() << std::endl;

    if (st.isError()) {
        aSession->methods().erase(input);
        return st;
    }

    methodInfo.context = std::move(v.front());
    v.pop_back();

    return Status(StatusCode::SUCCESS);
}

template<typename... Args>
Status registerSingleSignal(Private::SessionPrivate* aSession, std::string_view aSignalName) {

    if (aSession->info().name.empty() || aSession->info().path.empty()
        || aSession->info().interface.empty()) {
        return Status(StatusCode::INVALID_ARG);
    }

    std::string input = Method::getArgsString<Args...>();
    if (aSession->signals().count(aSignalName.data())) {
        return Status(StatusCode::NAME_EXISTS);
    }

    aSession->signals()[aSignalName.data()] = {};
    auto& sigInfo = aSession->signals()[aSignalName.data()];

    Private::VTableRegistrar reg(aSession->rawBus(), aSession->info().path, aSession->info().interface);
    reg.addSiganl(aSignalName, input);
    std::vector<std::unique_ptr<Private::VTableContext>> v;
    Status st = reg.commit(v);
    if (st.isError()) {
        aSession->signals().erase(input);
        return st;
    }

    sigInfo.context = std::move(v.front());
    return Status(StatusCode::SUCCESS);
}

template<typename... Args>
Status emitSignal(Private::SessionPrivate* aSession, std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, const Args&... aArgs) {
    Adaptor::RawBusMessagePtr rawSig =  Adaptor::RawMessage::createSignal(
        aSession->rawBus().get(), aPath, aIface, aSignal);
    if (!rawSig) {
        return Status(StatusCode::UNKNOWN_ERROR);
    }

    Status st = Status(StatusCode::SUCCESS);
    Private::MessagePrivate sig(Adaptor::RawMessageSharePtr(rawSig, true));
    if constexpr (sizeof...(Args)) {
        st = sig.write(aArgs...);
    }

    if (st.isError()) {
        return st;
    }

    return aSession->sendMessage(sig);
    
}

template<typename T>
struct PropertyWrapper {
    Private::SessionPrivate* session;
    std::string propName;
    T prop;
    std::string_view type;
    std::function<void(const T&)> onChange {nullptr};

    PropertyWrapper(Private::SessionPrivate* aSession, std::string aPropName, T aProp)
        : session(aSession)
        , propName(aPropName)
        , prop(aProp)
        , type(typeid(T).name()) {}

    T get() const {
        return prop;
    }

    void set(const T& aNew) {
        if (prop == aNew) {
            return;
        }

        prop = aNew;
        if (onChange) onChange(prop);

        auto& info = session->info();
        sd_bus_emit_properties_changed(
            session->rawBus().get(),
            info.path.c_str(), info.interface.c_str(),
            propName.c_str(), nullptr);
    }

    void onChanged(std::function<void(const T&)> aCallback) {
        onChange = std::move(aCallback);
    }

    static std::string signature() {
        return getSignature<T>();
    }

    static int onGetter(Adaptor::RawBusPtr, const char*, const char*, const char*,
        Adaptor::RawBusMessagePtr aReply, void* aData, Adaptor::RawBusErrorPtr aErr) {
                auto self = static_cast<PropertyWrapper*>(aData);
        Private::MessagePrivate reply(
            Adaptor::RawMessageSharePtr(aReply, false));
        
        Status st = reply.write(self->prop);
        if (st.isError()) {
            Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr); 
            return -1;
        }

        return 0;
    }

    static int onSetter(Adaptor::RawBusPtr, const char*, const char*, const char*,
        Adaptor::RawBusMessagePtr aValue, void* aData, Adaptor::RawBusErrorPtr aErr) {
        auto self = static_cast<PropertyWrapper*>(aData);
        Private::MessagePrivate msg(
            Adaptor::RawMessageSharePtr(aValue, false));
        
        T newVal{};
        Status st = msg.read(newVal);
        if (st.isError()) {
            Adaptor::RawRemoteError::fromStatus(st.code()).moveTo(aErr); 
            return -1;
        }

        self->set(std::move(newVal));
        return 0;
    }
};

// ========== 客户端：远程调用 ==========

template<typename... Args>
Private::MessagePrivate callSync(
    Private::SessionPrivate* aSession, uint64_t aTimeoutUmsc,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {

    Private::MessagePrivate callMsg = aSession->createMethodCall(
        aService, aPath, aIface, aMethod);

    Status st = Status(StatusCode::SUCCESS);
    if constexpr (sizeof...(Args)) {
        st = callMsg.write(aArgs...);
    }

    if (st.isError()) {
        Private::MessagePrivate errMsg(
            Adaptor::RawMessageSharePtr(nullptr));
        errMsg.setStatus(st);
        return errMsg;
    }

    Adaptor::RawBusMessagePtr rawReply = nullptr;
    Adaptor::RawRemoteError error;
    st = Adaptor::RawBus::callSync(
        aSession->rawBus().get(), callMsg.rawMessage(), aTimeoutUmsc,
        error.getRawPtr(), rawReply);
    Private::MessagePrivate repMsg(Adaptor::RawMessageSharePtr(rawReply, true));

    if (st.isError()) {
        repMsg.setStatus(error.toStatus());
    } else {
        repMsg.setStatus(st);
    }

    std::cout << "callSync -- aService:" << aService
        << ", aPath:" << aPath << ", aIface" << aIface
        << ", aMethod:" << aMethod << ", ret:" << repMsg.getStatus().message() << std::endl;

    return repMsg;
}

template<typename... Args>
Private::MessagePrivate callSync(
    Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {
    return callSync<>(aSession, 0, aService, aPath, aIface, aMethod, aArgs...);
}

template<typename... Args>
std::shared_ptr<Private::ReplyAsyncHandler> callAsync(
    Private::SessionPrivate* aSession, uint64_t aTimeoutUmsc,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {
    Private::MessagePrivate callMsg = aSession->createMethodCall(
        aService, aPath, aIface, aMethod);

    if constexpr (sizeof...(Args)) {
        callMsg.write(aArgs...);
    }

    auto repHandler = std::make_shared<Private::ReplyAsyncHandler>();
    Adaptor::RawBusSlotPtr rawSlot = nullptr;
    Status st = Adaptor::RawBus::callAsync(
        aSession->rawBus().get(), rawSlot, callMsg.rawMessage(),
        Private::ReplyAsyncHandler::onReply, repHandler.get(), aTimeoutUmsc
    );
    repHandler->mSlot = Adaptor::RawSlotSharePtr(rawSlot);
    repHandler->setStatus(st);

    std::cout << "callAsync -- aService:" << aService
        << ", aPath:" << aPath << ", aIface" << aIface
        << ", aMethod:" << aMethod << ", ret:" << st.message() << std::endl;

    return repHandler;
}

template<typename... Args>
std::shared_ptr<Private::ReplyAsyncHandler> callAsync(
    Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {
    return callAsync<>(aSession, 0, aService, aPath, aIface, aMethod, aArgs...);
}

template<typename Callback>
Status listenSignal(Private::SessionPrivate* aSession,
    std::string_view aSender, std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Callback&& aCallback) {

    auto handler = std::make_shared<Private::SignalHandler<Callback>>(
        std::forward<Callback>(aCallback));

    Status st = Adaptor::RawBus::listenSignal(
        aSession->rawBus().get(), handler->slot,
        aSender, aPath, aIface, aSignal,
        &Private::SignalHandler<Callback>::onSignal,
        handler.get()
    );

    if (st.isSuccess()) {
        aSession->addSignalHandler(std::move(handler));
    }

    return st;
}

Private::MessagePrivate getRemoteProperty(Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aProp) {
    return Private::PropertyHandler::getRemoteProperty(
        aSession, aService, aPath, aIface, aProp
    );
}

template<typename T>
Status setRemoteProperty(Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aProp, const T& aValue) {
    return Private::PropertyHandler::setRemoteProperty(
        aSession, aService, aPath, aIface, aProp, aValue);
}

template<typename Callback>
Status onRemotePropertyChanged(Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aProp, Callback&& aCallback) {

    std::string key = std::string(aService).append(":").append(aPath);
    Status st(StatusCode::SUCCESS);
    auto handler = aSession->getPropertyHandler(key);
    if (!handler) {
        auto newHandler = std::make_shared<Private::PropertyHandler>();
        newHandler->session = aSession;
        newHandler->service = std::string(aService);
        newHandler->path = std::string(aPath);
        st = Adaptor::RawBus::listenSignal(
            aSession->rawBus().get(), newHandler->slot,
            aService, aPath, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            &Private::PropertyHandler::onPropertyChanged, newHandler.get()
        );
        if (st.isError()) {
            return st;
        }
        
        aSession->setPropertyHandler(key, newHandler);
        handler = newHandler;
    }

    auto* propHandler = static_cast<Private::PropertyHandler*>(handler.get());
    propHandler->add(aIface, aProp, std::forward<Callback>(aCallback));

    return st;
}

}
}

#endif