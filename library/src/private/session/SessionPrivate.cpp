#include "private/session/SessionPrivate.hpp"


namespace Dbusxx {
namespace Private {
std::string SessionPrivate::ObjectInfo::makeKey(std::string_view path, std::string_view iface) {
    std::string key;
    key.reserve(path.size() + iface.size() + 1);
    key.append(path).append(":").append(iface);
    return key;
}

std::pair<std::string_view, std::string_view>
SessionPrivate::ObjectInfo::parseKey(std::string_view key) {
    auto pos = key.rfind(':');
    if (pos == std::string_view::npos) {
        return {key, {}};
    }

    return {key.substr(0, pos), key.substr(pos + 1)};
}

SessionPrivate::SessionPrivate(SessionType aType, std::string_view aServiceName, bool aIsServer)
    : mRawBus(makePrivate(aType, aServiceName, aIsServer, &mListenFd))
    , mType(aType)
    , mServiceName(aServiceName)
    , mIsServer(aIsServer) {
    setDaemonDeathWatcher();
}

SessionPrivate::~SessionPrivate() {
    if (mRawBus.get()) {
        Adaptor::RawBus::closeBus(mRawBus.get());
    }

    if (mListenFd >= 0) {
        close(mListenFd);
        mListenFd = -1;
    }
}

void SessionPrivate::addMethodEntry(const std::string &aKey,
    std::string_view aName, std::string aInput, std::string aOutput,
    Adaptor::RawBusMessageHandler aCallback, void *aData,
    std::shared_ptr<void> aDataHolder) {
    auto& obj = mRegisteredObjects[aKey];
    std::string regName = std::string(aName) + "_" + aInput;
    obj.methods[std::string(aName)] = {
        std::move(aDataHolder), std::move(aInput),
        std::move(aOutput), aCallback
    };
}

void SessionPrivate::addSignalEntry(const std::string& aKey,
    std::string_view aName, std::string aInput) {
    auto& obj = mRegisteredObjects[aKey];
    std::string regName = std::string(aName) + "_" + aInput;
    obj.signals[std::string(aName)] = { std::move(aInput) };
}

void SessionPrivate::addPropertyEntry(const std::string& aKey,
    std::string_view aName, std::string aSignature, bool aWritable,
    Adaptor::RawBusPropertyGetter aGetter, Adaptor::RawBusPropertySetter aSetter,
    void* aData, std::shared_ptr<void> aDataHolder) {
    auto& obj = mRegisteredObjects[aKey];
    obj.properties[std::string(aName)] = {
        std::move(aDataHolder), std::move(aSignature),
        aWritable, aGetter, aSetter
    };
}

Status SessionPrivate::commitBuilder(const std::string& aKey) {
    auto it = mRegisteredObjects.find(aKey);
    if (it == mRegisteredObjects.end()) {
        return Status(StatusCode::INVALID_ARG);
    }

    //! Release old slot first to force sd-bus to clean up strdup'd strings
    //! from the old vtable. Otherwise sd_bus_add_object_vtable may
    //! access freed strings during replacement (heap-use-after-free).
    auto& obj = it->second;
    if (obj.context) {
        obj.context->slot = Adaptor::RawSlotSharePtr();
    }

    auto [path, iface] = ObjectInfo::parseKey(aKey);
    Adaptor::VTableRegistrar reg(mRawBus, path, iface);
    for (auto& [name, entry] : obj.methods) {
        reg.addMethod(name, entry.input, entry.output,
            entry.callback, entry.data.get());
    }

    for (auto& [name, entry] : obj.signals) {
        reg.addSiganl(name, entry.input);
    }

    for (auto& [name, entry] : obj.properties) {
        reg.addProperty(name, entry.signature,
            entry.getter, entry.setter,
            entry.data.get(), entry.writable);
    }

    std::unique_ptr<Adaptor::VTableContext> ctx;
    auto st = reg.commit(ctx);
    if (st.isError()) {
        return st;
    }

    obj.context = std::move(ctx);
    return Status(StatusCode::SUCCESS);
}

Status SessionPrivate::reconnect() {
    //! Clear all slots
    for (auto& [name, object] : mRegisteredObjects) {
        object.context->slot = Adaptor::RawSlotSharePtr();
    }
    for (auto& inf : mRegisteredSigHandlers) {
        inf.slot = Adaptor::RawSlotSharePtr();
    }
    for (auto& [key, inf] : mRegisteredPropHandlers) {
        inf.slot = Adaptor::RawSlotSharePtr();
    }

    if (mRawBus) {
        Adaptor::RawBus::closeBus(mRawBus.get());
    }

    mRawBus = makePrivate(mType, mServiceName, mIsServer, &mListenFd);
    if (!mRawBus && mListenFd < 0) {
        //! User/System Session: mRawBus != nullptr
        //! Peer Session: mListenFd >= 0
        return Status(StatusCode::NOT_CONNECTED);
    }

    setDaemonDeathWatcher();
    if ((mType == SessionType::USER || mType == SessionType::SYSTEM)
        && !mServiceName.empty()) {
        return requestNameToDaemon();
    }

    return Status(StatusCode::SUCCESS);
}

Status SessionPrivate::requestNameToDaemon() {
    return Adaptor::RawBus::setUniqueName(
        mRawBus.get(), mServiceName, 0);
}

MessagePrivate SessionPrivate::createReply(MessagePrivate& aCallMsg) {
    auto reply = Adaptor::RawMessageSharePtr::createReply(
        aCallMsg.rawMessage());
    return MessagePrivate(reply);
}

MessagePrivate SessionPrivate::createMethodCall(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod) {

    auto call =  Adaptor::RawMessageSharePtr::createMethodCall(
        mRawBus.get(), aService, aPath, aIface, aMethod 
    );

    return MessagePrivate(call);
}

Status SessionPrivate::sendMessage(MessagePrivate& aMsg) {
    if (!mRawBus) {
        return Status(StatusCode::UNKNOWN_ERROR);
    }

    return Adaptor::RawBus::sendMessage(mRawBus.get(), aMsg.rawMessage());
}

Status SessionPrivate::sendMessage(MessagePrivate& aMsg, std::string_view aDestination) {
    if (!mRawBus) {
        return Status(StatusCode::UNKNOWN_ERROR);
    }

    return Adaptor::RawBus::sendMessage(mRawBus.get(), aMsg.rawMessage(), aDestination);
}

int SessionPrivate::process() {
    return Adaptor::RawBus::process(mRawBus.get(), nullptr);
}

int SessionPrivate::wait(uint64_t aTimeoutMs) {
    return Adaptor::RawBus::wait(mRawBus.get(), aTimeoutMs);
}

int SessionPrivate::getFd() const {
    return Adaptor::RawBus::getFd(mRawBus.get());
}

void SessionPrivate::flush() {
    Adaptor::RawBus::flushBus(mRawBus.get());
}

Status SessionPrivate::acceptPeerConnection(Adaptor::RawBusEventPtr aEvent) {
    Adaptor::RawBusPtr bus = nullptr;
    Status st = Adaptor::RawBus::acceptConnection(bus, mListenFd, aEvent);
    if (st.isError()) {
        return st;
    }

    mRawBus = Adaptor::RawBusSharePtr(bus, true);
    if (mPeerAcceptedCb) {
        return mPeerAcceptedCb();
    }

    return Status(StatusCode::SUCCESS);
}

Adaptor::RawBusSharePtr SessionPrivate::makePrivate(SessionType aType,
    std::string_view aServiceName, bool aIsServer, int* aListenFd) {
    switch (aType) {
        case SessionType::USER:
            return Adaptor::RawBusSharePtr::makeUser();
        case SessionType::SYSTEM:
            return Adaptor::RawBusSharePtr::makeSystem();
        case SessionType::PEER:
            return Adaptor::RawBusSharePtr::makePeer(aServiceName, aIsServer, aListenFd);
        default:
            return Adaptor::RawBusSharePtr(nullptr, false);
    }
}

void SessionPrivate::setDaemonDeathWatcher() {
    if (mType == SessionType::PEER) {
        return;
    }

    Adaptor::RawBus::setWatchBind(mRawBus.get(), true);
    Adaptor::RawBus::setExitOnDisconnect(mRawBus.get(), false);
    Adaptor::RawBus::setConnectedSignal(mRawBus.get(), true);
}
}
}