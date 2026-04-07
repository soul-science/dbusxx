#include "DbusMessage.hpp"

#include "DbusSession.hpp"

namespace SSDbus {

std::unique_ptr<DbusSession> DbusMessage::getDbus() {
    return std::make_unique<DbusSession>(sd_bus_message_get_bus(mRawMsg), false);
}

}
