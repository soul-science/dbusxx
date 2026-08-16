#ifndef SSDBUS_RECONNECT_HPP
#define SSDBUS_RECONNECT_HPP

#include "Status.hpp"


namespace SSDbus {
namespace Private {
    class SessionPrivate;
}
namespace Method {
Status reconnectSession(Private::SessionPrivate* aSession);
}
}

#endif