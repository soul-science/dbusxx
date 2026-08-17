#include "Session.hpp"


namespace Dbusxx {
Session::Session(SessionType aType,
    std::string_view aServiceName, bool aIsServer)
    : mPrivate(std::make_shared<Private::SessionPrivate>(
        aType, aServiceName, aIsServer))
    , mRepsPtr(std::make_shared<PendingRepsV>()) {}

Session Session::systemSession() {
    return Session(SessionType::SYSTEM);
}

Session Session::systemSession(std::string_view aServiceName) {
    Session s(SessionType::SYSTEM, aServiceName);
    s.mPrivate->requestNameToDaemon();
    return s;
}

Session Session::userSession() {
    return Session(SessionType::USER);
}

Session Session::userSession(std::string_view aServiceName) {
    Session s(SessionType::USER, aServiceName);
    s.mPrivate->requestNameToDaemon();
    return s;
}

Session Session::peerSession(std::string_view aServiceName, bool aIsServer) {
    return Session(SessionType::PEER, aServiceName, aIsServer);
}

Session Session::createSession(SessionType aType,
    std::string_view aServiceName, bool aIsServer) {
    switch (aType) {
        case SessionType::SYSTEM:
            return aServiceName.empty() ?
                systemSession() : systemSession(aServiceName);
        case SessionType::PEER:
            return peerSession(aServiceName, aIsServer);
        case SessionType::USER:
        default:
            return aServiceName.empty() ?
                userSession() : userSession(aServiceName);
    }
}

int Session::process() {
    return mPrivate->process();
}

int Session::wait(uint64_t aTimeoutUsec) {
    return mPrivate->wait(aTimeoutUsec);
}

void Session::flush() {
    mPrivate->flush();
}

Session::RegisterBuilder Session::registerBuilder(std::string_view aPath, std::string_view aIface) {
    return RegisterBuilder(
        mPrivate.get(),
        Private::SessionPrivate::ObjectInfo::makeKey(aPath, aIface),
        std::string(aPath), std::string(aIface)
    );
}
}