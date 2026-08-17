#ifndef DBUSXX_RECONNECT_HPP
#define DBUSXX_RECONNECT_HPP

#include "Status.hpp"


namespace Dbusxx {
namespace Private {
    class SessionPrivate;
}
namespace Method {
Status reconnectSession(Private::SessionPrivate* aSession);
}
}

#endif