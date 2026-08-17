#ifndef DBUSXX_RAW_ADAPTOR_HPP
#define DBUSXX_RAW_ADAPTOR_HPP

#include <cassert>
#include <cerrno>
#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>
#include <systemd/sd-event.h>
#include <string>
#include <string_view>
#include <stdexcept>

#include "Status.hpp"


namespace Dbusxx {
namespace Adaptor {
using RawBus_ = sd_bus;
using RawBusPtr = sd_bus*;
using RawbusMessage_ = sd_bus_message;
using RawBusMessagePtr = sd_bus_message*;
using RawBusSlotPtr = sd_bus_slot*;
using RawBusCredsPtr = sd_bus_creds*;
using RawBusTrackPtr = sd_bus_track*;
using RawBusError = sd_bus_error;
using RawBusErrorPtr = sd_bus_error*;
using RawBusErrorMap = sd_bus_error_map;
using RawBusEventPtr = sd_event*;
using RawBusEventSrcPtr = sd_event_source*;
using RawBusVTable = sd_bus_vtable;

using RawBusMessageHandler = sd_bus_message_handler_t;
using RawBusPropertyGetter = sd_bus_property_get_t;
using RawBusPropertySetter = sd_bus_property_set_t;
using RawBusObjectFinder = sd_bus_object_find_t;
using RawBusNodeEnumerator = sd_bus_node_enumerator_t;
using RawBusTrackHandler = sd_bus_track_handler_t;
using RawDeleterCallback = sd_bus_destroy_t;
using RawEventIOHandler = sd_event_io_handler_t;

namespace RawCheck {
inline bool isInterfaceNameValid(std::string_view aName) {
    return sd_bus_interface_name_is_valid(aName.data()) > 0;
}

inline bool isInterfaceNameValid(const std::string& aName) {
    return isInterfaceNameValid(aName);
}

inline bool isServiceNameValid(std::string_view aName) {
    return sd_bus_service_name_is_valid(aName.data()) > 0;
}

inline bool isServiceNameValid(const std::string& aName) {
    return isServiceNameValid(aName);
}

inline bool isMemberNameValid(std::string_view aName) {
    return sd_bus_member_name_is_valid(aName.data()) > 0;
}

inline bool isMemberNameValid(const std::string& aName) {
    return isMemberNameValid(aName);
}

inline bool isPathNameValid(std::string_view aName) {
    return sd_bus_object_path_is_valid(aName.data()) > 0;
}

inline bool isPathNameValid(const std::string& aName) {
    return isPathNameValid(aName);
}
}
}
}

#endif