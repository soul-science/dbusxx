#ifndef SSDBUS_RECONNECT_HPP
#define SSDBUS_RECONNECT_HPP

#include "message/PropertyHandler.hpp"
#include "session/SessionPrivate.hpp"
#include "Status.hpp"
#include "Utils.hpp"

namespace SSDbus {
namespace Method {

Status reconnectSession(Private::SessionPrivate* aSession) {
    Status st = aSession->reconnect();
    if (st.isError()) {
        return st;
    }

    Adaptor::RawBusPtr bus = aSession->rawBus().get();
    const char* path = aSession->info().path.c_str();
    const char* iface = aSession->info().interface.c_str();

    for (auto& [name, inf] : aSession->methods()) {
        Adaptor::RawBusSlotPtr newSlot = nullptr;
        st = Adaptor::RawBus::addObjectToVTable(
            bus, newSlot, path, iface,
            inf.context->vtable.get(),
            inf.data.get()
        );
        if (st.isError()) {
            return st;
        }
        inf.context->slot = Adaptor::RawSlotSharePtr(newSlot);
    }

    for (auto& [name, inf] : aSession->signals()) {
        Adaptor::RawBusSlotPtr newSlot = nullptr;
        st = Adaptor::RawBus::addObjectToVTable(
            bus, newSlot, path, iface,
            inf.context->vtable.get(), nullptr);
        if (st.isError()) {
            return st;
        }
        inf.context->slot = Adaptor::RawSlotSharePtr(newSlot);
    }

    for (auto& [name, inf] : aSession->properties()) {
        Adaptor::RawBusSlotPtr newSlot = nullptr;
        st = Adaptor::RawBus::addObjectToVTable(
            bus, newSlot, path, iface,
            inf.context->vtable.get(), inf.data.get());
        if (st.isError()) {
            return st;
        }
        inf.context->slot = Adaptor::RawSlotSharePtr(newSlot);
    }

    for (auto& inf : aSession->signalHandlers()) {
        Adaptor::RawBusSlotPtr raw = nullptr;
        st = Adaptor::RawBus::listenSignal(
            bus, raw,
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
            bus, raw,
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

#endif