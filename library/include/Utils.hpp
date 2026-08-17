#ifndef DBUSXX_UTILS_HPP
#define DBUSXX_UTILS_HPP

#include <unistd.h>


namespace Dbusxx {
enum class SessionType : uint8_t {
    SYSTEM,
    USER,
    PEER,
    INVALID
};

}

#endif //! DBUSXX_UTILS_HPP