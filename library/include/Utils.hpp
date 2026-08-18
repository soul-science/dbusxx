#ifndef DBUSXX_UTILS_HPP
#define DBUSXX_UTILS_HPP

#include <cstdint>
#include <unistd.h>


namespace Dbusxx {
//! @brief Type of D-Bus connection a session is bound to.
enum class SessionType : uint8_t {
    SYSTEM,     //!< System bus
    USER,       //!< User/session bus
    PEER,       //!< Peer-to-peer connection (no bus daemon)
    INVALID     //!< Uninitialized / invalid
};
}

#endif //! DBUSXX_UTILS_HPP