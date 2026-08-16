#include "private/message/PropertyHandler.hpp"

namespace SSDbus {
namespace Private {

Private::MessagePrivate PropertyHandler::getRemoteProperty(Private::SessionPrivate* aSession,
    std::string_view aService, std::string_view aPath, std::string_view aIface,
    std::string_view aProp) {
    Private::MessagePrivate callMsg = aSession->createMethodCall(
        aService, aPath, "org.freedesktop.DBus.Properties", "Get");

    Status st = callMsg.write(aIface, aProp);
    if (st.isError()) {
        Private::MessagePrivate errMsg(
            Adaptor::RawMessageSharePtr(nullptr));
        errMsg.setStatus(st);
        return errMsg;
    }

    Adaptor::RawBusMessagePtr rawReply = nullptr;
    Adaptor::RawRemoteError error;
    st = Adaptor::RawBus::callSync(
        aSession->rawBus().get(), callMsg.rawMessage(), 0,
        error.getRawPtr(), rawReply);
    Private::MessagePrivate repMsg(Adaptor::RawMessageSharePtr(rawReply, true));

    if (st.isError()) {
        repMsg.setStatus(error.toStatus());
    } else {
        repMsg.setStatus(st);
    }
    
    return repMsg;
}


int PropertyHandler::onPropertyChanged(Adaptor::RawBusMessagePtr aMsg, void* aUsr, Adaptor::RawBusErrorPtr) {
    auto* self = static_cast<PropertyHandler*>(aUsr);
    //! Read sa{sv}as from ".PropertiesChanged"
    Private::MessagePrivate message(Adaptor::RawMessageSharePtr(aMsg, false));

    //! Read s -> interface name
    std::string iface;
    message.read(iface);
    auto ifaceProps = self->callbacks.find(iface);
    if (ifaceProps == self->callbacks.end()) {
        return 0;
    }

    //! Read a{sv}
    Status st = Adaptor::RawMessage::enterContainer(
        message.rawMessage(), SD_BUS_TYPE_ARRAY, "{sv}");
    if (st.isError()) {
        return 0;
    }

    while (!Adaptor::RawMessage::isEnd(message.rawMessage(), false)) {
        st = Adaptor::RawMessage::enterContainer(
            message.rawMessage(), SD_BUS_TYPE_DICT_ENTRY, nullptr);
        if (st.isError()) {
            break;
        }

        std::string key;
        message.read(key);
        auto it = ifaceProps->second.find(key);
        if (it != ifaceProps->second.end()) {
            it->second(message);
        } else {
            Adaptor::RawMessage::skip(message.rawMessage(), nullptr);
        }

        st = Adaptor::RawMessage::exitContainer(message.rawMessage());
        if (st.isError()) {
            break;
        }
    }

    Adaptor::RawMessage::exitContainer(message.rawMessage());

    //! read "as"
    st = Adaptor::RawMessage::enterContainer(
        message.rawMessage(), SD_BUS_TYPE_ARRAY, "s");
    if (st.isError()) {
        return 0;
    }

    while (!Adaptor::RawMessage::isEnd(message.rawMessage(), false)) {
        std::string key;
        message.read(key);
        auto it = ifaceProps->second.find(key);
        if (it != ifaceProps->second.end()) {
            auto reply = getRemoteProperty(
                self->session, self->service, self->path, iface, key);
            it->second(reply);
        }
    }

    Adaptor::RawMessage::exitContainer(message.rawMessage());

    return 0;
}
}
}