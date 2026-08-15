#include "Client.hpp"


namespace SSDbus {
Client::AsyncPool::AsyncPool(Session s)
    : session(std::move(s))
    , looper(session)
    , thread(&Looper::run, &looper) {}

Client::AsyncPool::~AsyncPool() {
    looper.stop();
    thread.join();
}

Client::Client(SessionType aType, std::string aService,
    std::string aPath, std::string aInterface)
    : mAsyncPool(getAsyncPool(aType, aService))
    , mAsyncPtr(&mAsyncPool->session)
    , mLooper(&mAsyncPool->looper)
    , mType(aType)
    , mInfo({aService, aPath, aInterface}) {}

Client::Client(Looper& aLooper, std::string aService,
    std::string aPath, std::string aInterface)
    : mLooper(&aLooper)
    , mAsyncPtr(aLooper.session())
    , mType(aLooper.session()->type())
    , mInfo({aService, aPath, aInterface}) {}

Client::Client(Client&& aOther) noexcept
    : mAsyncPool(std::move(aOther.mAsyncPool))
    , mAsyncPtr(aOther.mAsyncPtr)
    , mLooper(aOther.mLooper)
    , mType(aOther.mType)
    , mInfo(std::move(aOther.mInfo)) {
    aOther.mLooper = nullptr;
    aOther.mAsyncPtr = nullptr;
}

Client& Client::operator=(Client&& aOther) noexcept {
    if (this == &aOther) {
        return *this;
    }

    mAsyncPool = std::move(aOther.mAsyncPool);
    mAsyncPtr = aOther.mAsyncPtr;
    mLooper = aOther.mLooper;
    mType = aOther.mType;
    mInfo = std::move(aOther.mInfo);

    aOther.mLooper = nullptr;
    aOther.mAsyncPtr = nullptr;
    return *this;
}

Session Client::createSession(SessionType aType, std::string_view aService) {
    switch (aType) {
        case SessionType::SYSTEM:
            return Session::systemSession();
        case SessionType::PEER:
            return Session::peerSession(aService);
        case SessionType::USER:
        default:
            return Session::userSession();
    }
}

std::shared_ptr<Client::AsyncPool> Client::getAsyncPool(
    SessionType aType, std::string_view aService) {
    static std::weak_ptr<AsyncPool> sUserPool;
    static std::weak_ptr<AsyncPool> sSystemPool;
    static std::mutex sPeerLock;
    static std::map<std::string, std::weak_ptr<AsyncPool>> sPeerPools;

    if (aType == SessionType::PEER) {
        std::lock_guard lock(sPeerLock);
        auto& weak = sPeerPools[aService.data()];
        auto sp = weak.lock();
        if (!sp) {
            weak = sp =
                std::make_shared<AsyncPool>(createSession(aType, aService));
        }

        return sp;
    }

    auto& weak = (aType == SessionType::SYSTEM) ? sSystemPool : sUserPool;
    auto sp = weak.lock();
    if (!sp) {
        weak = sp = std::make_shared<AsyncPool>(
            createSession(aType, aService));
    }

    return sp;
}

}