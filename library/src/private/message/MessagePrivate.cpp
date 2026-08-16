#include "private/message/MessagePrivate.hpp"


namespace SSDbus {
namespace Private {
MessagePrivate::MessagePrivate(Adaptor::RawMessageSharePtr aPtr)
    : mRawMsg(aPtr) {}

std::string MessagePrivate::getSender() const {
    return Adaptor::RawMessage::getSender(mRawMsg.get());
}
}
}