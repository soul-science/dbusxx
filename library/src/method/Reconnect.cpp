#include "method/Reconnect.hpp"

#include "adaptor/VTableRegistrar.hpp"
#include "message/PropertyHandler.hpp"
#include "session/SessionPrivate.hpp"
#include "Utils.hpp"

namespace SSDbus {
namespace Method {

Status reconnectSession(Private::SessionPrivate* aSession) {
    Status st = aSession->reconnect();
    if (st.isError()) {
        return st;
    }

    Adaptor::RawBusSharePtr busSp = aSession->rawBus();
    for (auto& [key, object] : aSession->objects()) {
        auto [path, iface] = Private::SessionPrivate::ObjectInfo::parseKey(key);
        Adaptor::VTableRegistrar reg(busSp, path, iface);
        for (auto& [name, entry] : object.methods) {
            reg.addMethod(name, entry.input, entry.output,
                entry.callback, entry.data.get());
        }

        for (auto& [name, entry] : object.signals) {
            reg.addSiganl(name, entry.input);
        }

        for (auto& [name, entry] : object.properties) {
            reg.addProperty(name, entry.signature,
                entry.getter, entry.setter,
                entry.data.get(), entry.writable);
        }

        std::unique_ptr<Adaptor::VTableContext> ctx;
        st = reg.commit(ctx);
        if (st.isError()) {
            return st;
        }
        object.context = std::move(ctx);
    }

    for (auto& inf : aSession->signalHandlers()) {
        Adaptor::RawBusSlotPtr raw = nullptr;
        st = Adaptor::RawBus::listenSignal(
            busSp.get(), raw,
            inf.sender, inf.path, inf.iface,
            inf.signal, inf.callback, inf.data.get());
        if (st.isError()) {
            return st;
        }
        inf.slot = Adaptor::RawSlotSharePtr(raw);
    }

    for (auto& [key, inf] : aSession->propertyHandlers()) {
        auto* handler = static_cast<Private::PropertyHandler*>(inf.handler.get());
        Adaptor::RawBusSlotPtr raw = nullptr;
        st = Adaptor::RawBus::listenSignal(
            busSp.get(), raw,
            handler->service, handler->path,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            &Private::PropertyHandler::onPropertyChanged, handler);
        if (st.isError()) {
            return st;
        }
        inf.slot = Adaptor::RawSlotSharePtr(raw);
    }

    return Status(StatusCode::SUCCESS);
}

}
}