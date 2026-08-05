#ifndef SSDBUS_PROPERTY_HANDLER_HPP
#define SSDBUS_PROPERTY_HANDLER_HPP

#include <string>
#include <unordered_map>

#include "adaptor/DbusArgs.hpp"
#include "adaptor/RawCommon.hpp"
#include "method/FunctionTrait.hpp"
#include "message/MessagePrivate.hpp"
#include "session/SessionPrivate.hpp"
#include "Status.hpp"


namespace SSDbus {
namespace Private {

struct PropertyHandler {
    using PropCallback = std::function<void(MessagePrivate&)>;

    Private::SessionPrivate* session { nullptr };
    std::string service;
    std::string path;
    std::unordered_map<std::string,
        std::unordered_map<std::string, PropCallback>> callbacks;

    template<typename Callback>
    void add(std::string_view aIface, std::string_view aProp, Callback&& aCb) {
        using traits = Method::FuncTrait<std::decay_t<Callback>>;
        using ArgType = std::decay_t<std::tuple_element_t<0, typename traits::ArgsTuple>>;
        
        callbacks[std::string(aIface)][std::string(aProp)] =
            [cb = std::forward<Callback>(aCb)] (MessagePrivate& aMsg) -> void {
                ArgType val{};
                if (aMsg.read(val).isSuccess()) {
                    cb(val);
                }
            };
    }

    static Private::MessagePrivate getRemoteProperty(Private::SessionPrivate* aSession,
        std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aProp);

    template<typename T>
    static Status setRemoteProperty(Private::SessionPrivate* aSession,
        std::string_view aService, std::string_view aPath, std::string_view aIface,
        std::string_view aProp, const T& aValue) {
        Private::MessagePrivate callMsg = aSession->createMethodCall(
            aService, aPath, "org.freedesktop.DBus.Properties", "Set");

        //! Properties.Set 签名 "ssv": STRING iface, STRING prop, VARIANT value
        Status st = callMsg.write(aIface, aProp);
        if (st.isError()) {
            return st;
        }

        st = Adaptor::RawMessage::openContainer(
            callMsg.rawMessage(), SD_BUS_TYPE_VARIANT, getSignature<T>().c_str());
        if (st.isError()) {
            return st;
        }

        st = callMsg.write(aValue);
        if (st.isError()) {
            return st;
        }

        st = Adaptor::RawMessage::closeContainer(callMsg.rawMessage());
        if (st.isError()) {
            return st;
        }

        Adaptor::RawBusMessagePtr rawReply = nullptr;
        Adaptor::RawRemoteError error;
        st = Adaptor::RawBus::callSync(
            aSession->rawBus().get(), callMsg.rawMessage(), 0,
            error.getRawPtr(), rawReply);
        Private::MessagePrivate repMsg(Adaptor::RawMessageSharePtr(rawReply, true));

        if (st.isError()) {
            return error.toStatus();
        }
        
        return st;    
    }

    static int onPropertyChanged(Adaptor::RawBusMessagePtr aMsg, void* aUsr, Adaptor::RawBusErrorPtr);
};

}
}


#endif