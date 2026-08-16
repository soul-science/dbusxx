#ifndef SSDBUS_UTILS_HPP
#define SSDBUS_UTILS_HPP

#include <unistd.h>


namespace SSDbus {
enum class SessionType : uint8_t {
    SYSTEM,
    USER,
    PEER,
    INVALID
};

}

#endif //! SSDBUS_UTILS_HPP