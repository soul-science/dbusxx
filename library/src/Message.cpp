#include "Message.hpp"


namespace Dbusxx {
Message::Message(std::shared_ptr<Private::MessagePrivate> aImpl)
    : mPrivate(std::move(aImpl)) {}

Message::Message(Private::MessagePrivate&& aImpl)
    : Message(std::make_shared<Private::MessagePrivate>(std::move(aImpl))) {}
}