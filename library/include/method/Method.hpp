#ifndef SSDBUS_DBUS_METHOD_HPP
#define SSDBUS_DBUS_METHOD_HPP


#include <iostream>

#include "Status.hpp"
#include "message/MessagePrivate.hpp"
#include "message/SignalHandler.hpp"
#include "session/SessionPrivate.hpp"

namespace SSDbus {
namespace Method {

// ========== 共享工具 ==========

template<typename Cls, typename Ret, typename... Args>
std::string getArgsString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
    std::string args;
    if constexpr (sizeof...(Args)) {
        (args.append(std::string(1, SSDbus::getSignature<Args>())), ...);
    }

    return args;
}

template<typename Cls, typename Ret, typename... Args>
std::string getReturnString(Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
    return std::string(1, SSDbus::getSignature<Ret>());
}

template<typename... Args>
std::string getArgsString() {
    std::string args;
    if constexpr (sizeof...(Args)) {
        (args.append(std::string(1, getSignature<Args>())), ...);
    }
    return args;
}

// ========== 服务端：回调上下文 + 包装器 ==========

template <typename Cls, typename Ret, typename... Args>
struct CallContext {
    using MethodPtr = std::shared_ptr<std::pair<Cls*, Ret (Cls::*)(Args...)>>;

    CallContext(Private::SessionPrivate* aSession, MethodPtr aMethodPtr)
        : session(aSession)
        , method(std::move(aMethodPtr)) {}
    
    Private::SessionPrivate* session;
    MethodPtr method;
};

template <typename Cls, typename Ret, typename... Args>
struct IMethodWrapper {
    using ClsFuncPtr = Ret (Cls::*)(Args...);

    static int call(Adaptor::RawBusMessagePtr aMsg, void* aUsrData, Adaptor::RawBusErrorPtr aErr) {
        auto* context = static_cast<CallContext<Cls, Ret, Args...>*>(aUsrData);
        Cls* obj = context->method->first;
        ClsFuncPtr func = context->method->second;

        Private::MessagePrivate message(Adaptor::RawMessageSharePtr(aMsg, false));
        Private::MessagePrivate reply = context->session->createReply(message);

        if constexpr (sizeof...(Args)) {
            //! Parse args from message
            std::tuple<typename ArgTypeAdaptor<std::decay_t<Args>>::type...> tpl;
            message.read(tpl);

            //! Apply function
            if constexpr (std::is_same_v<Ret, void>) {
                std::apply(
                    [&](auto&&... aArgs) -> void {
                        (obj->*func)(std::forward<decltype(aArgs)>(aArgs)...);
                    },
                    tpl
                );

            } else {
                Ret res = std::apply(
                    [&](auto&&... aArgs) -> Ret {
                        return (obj->*func)(std::forward<decltype(aArgs)>(aArgs)...);
                    },
                    tpl
                );

                reply.write(res);
            }
        } else {
            //! Apply function
            if constexpr (std::is_same_v<Ret, void>) {
                (obj->*func)();
            } else {
                Ret res = (obj->*func)();
                reply.write(res);
            }
        }

        //! Response reply
        auto st = context->session->sendMessage(reply, message.getSender());
        return st.isSuccess() ? 1 : -1;
    }
};

// ========== 服务端：注册方法 ==========

template<typename Cls, typename Ret, typename... Args>
Status registerMethod(
    Private::SessionPrivate* aSession, std::string_view aFuncName, Cls* aObj, Ret (Cls::*aFunc)(Args...)) {
    using ClsFuncPtr = Ret (Cls::*)(Args...);

    if (aSession->info().name.empty() || aSession->info().path.empty()) {
        return Status(StatusCode::INVALID_ARG);
    }

    auto method = std::make_shared<std::pair<Cls*, ClsFuncPtr>>(aObj, aFunc);
    auto data = std::make_shared<Method::CallContext<Cls, Ret, Args...>>(
        aSession, method
    );
    auto dataPtr = data.get();

    aSession->methods()[aFuncName.data()] = {};
    auto& info = aSession->methods()[aFuncName.data()];

    info.input = Method::getArgsString(aObj, aFunc);
    info.output = Method::getReturnString(aObj, aFunc);

    std::cout << "input: " << info.input << ", output:" << info.output << std::endl;

    //! Create vtable
    using wrapper = IMethodWrapper<Cls, Ret, Args...>;
    auto vtable = std::unique_ptr<Adaptor::RawBusVTable[]>( new Adaptor::RawBusVTable[3] {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD(aFuncName.data(), info.input.c_str(), info.output.c_str(), &wrapper::call, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_VTABLE_END
    });

    Adaptor::RawBusVTable* vtablePtr = vtable.get();
    Adaptor::RawBusSlotPtr rawSlot { nullptr };
    auto ret = sd_bus_add_object_vtable(
        aSession->rawBus(),
        &rawSlot, aSession->info().path.c_str(),
        aSession->info().interface.c_str(), vtablePtr, dataPtr
    );

    Slot slot(rawSlot);
    if (ret < 0) {
        aSession->methods().erase(aFuncName.data());
        return Adaptor::RawError::makeStatus(ret);
    }

    info.method = data;
    info.vtable = std::move(vtable);
    info.slot = std::move(slot);

    return Status(StatusCode::SUCCESS);
}

// template<typename... Args>
// Message emitSignal(
//     Session& aSession, std::string_view aSignal
// );

// ========== 客户端：远程调用 ==========

template<typename Ret, typename... Args>
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
    st = Adaptor::RawBus::callSync(
        aSession->rawBus(), callMsg.rawMessage(), aTimeoutUmsc,
        nullptr /* error */, rawReply);
    Private::MessagePrivate repMsg(Adaptor::RawMessageSharePtr(rawReply, true));
    repMsg.setStatus(st);
    std::cout << "callSync -- aService:" << aService
        << ", aPath:" << aPath << ", aIface" << aIface
        << ", aMethod:" << aMethod << ", ret:" << st.message() << std::endl;

    return repMsg;
}

template<typename Ret, typename... Args>
Private::MessagePrivate callSync(
    Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {
    return callSync<Ret>(aSession, 0, aService, aPath, aIface, aMethod, aArgs...);
}

template<typename Ret, typename... Args>
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
    Status st = Adaptor::RawBus::callAsync(
        aSession->rawBus(), nullptr, callMsg.rawMessage(),
        Private::ReplyAsyncHandler::onReply, repHandler.get(), aTimeoutUmsc
    );
    repHandler->setStatus(st);

    std::cout << "callAsync -- aService:" << aService
        << ", aPath:" << aPath << ", aIface" << aIface
        << ", aMethod:" << aMethod << ", ret:" << st.message() << std::endl;

    return repHandler;
}

template<typename Ret, typename... Args>
std::shared_ptr<Private::ReplyAsyncHandler> callAsync(
    Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aMethod, const Args&... aArgs) {
    return callAsync<Ret>(aSession, 0, aService, aPath, aIface, aMethod, aArgs...);
}

template<typename Callback>
Status listenSignal(Private::SessionPrivate* aSession,
    std::string_view aSender, std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Callback&& aCallback) {

    auto handler = std::make_shared<Private::SignalHandler<Callback>>(
        std::forward<Callback>(aCallback));

    Status st = Adaptor::RawBus::listenSignal(
        aSession->rawBus(), handler->slot,
        aSender, aPath, aIface, aSignal,
        &Private::SignalHandler<Callback>::onSignal,
        handler.get()
    );

    if (st.isSuccess()) {
        aSession->addSignalHandler(std::move(handler));
    }

    return st;
}

}
}


#endif