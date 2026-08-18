#include "private/adaptor/RawBusSharePtr.hpp"

#include <cstring>

#include "private/adaptor/RawRemoteError.hpp"


namespace Dbusxx {
namespace Adaptor {
namespace RawBus {
namespace {
struct PeerSignalFilter {
    std::string path;
    std::string iface;
    std::string signal;
    RawBusMessageHandler callback;
    void* data;

    static int onMessage(sd_bus_message* aMsg, void* aData, sd_bus_error*);

    inline static void destroy(void* d) {
        delete static_cast<PeerSignalFilter*>(d);
    }
};
}

void unrefBus(RawBusPtr aBus) {
    if (!aBus) {
        return;
    }

    sd_bus_unref(aBus);
}

void refBus(RawBusPtr aBus) {
    if (!aBus) {
        return;
    }

    sd_bus_ref(aBus);
}

bool isBusReady(RawBusPtr aBus) {
    if (!aBus) {
        return false;
    }

    return sd_bus_is_ready(aBus) > 0;
}

bool isBusOpen(RawBusPtr aBus) {
    if (!aBus) {
        return false;
    }

    return sd_bus_is_open(aBus) > 0;
}

Status openSystemBus(RawBusPtr& aBus) {
    return RawErrorConvert::makeStatus(sd_bus_open_system(&aBus));
}

Status openUserBus(RawBusPtr& aBus) {
    return RawErrorConvert::makeStatus(sd_bus_open_user(&aBus));
}

Status openPeerBus(RawBusPtr& aBus, std::string_view aSocket,
    bool aIsServer, int* aListenFd) {
    //! Parse "unix:path=/xxx" or "unix:abstract=xxx"
    bool isAbstract = false;
    std::string_view path;
    if (aSocket.rfind("unix:path=", 0) == 0) {
        path = aSocket.substr(10);
    } else if (aSocket.rfind("unix:abstract=", 0) == 0) {
        path = aSocket.substr(14);
        isAbstract = true;
    } else {
        return Status(StatusCode::INVALID_ARG);
    }

    if (path.empty()) {
        return Status(StatusCode::INVALID_ARG);
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (isAbstract) {
        addr.sun_path[0] = '\0';
        snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1,
            "%.*s", static_cast<int>(path.size()), path.data());
    } else {
        //! "%.*s" copy the following input size from the path
        snprintf(addr.sun_path, sizeof(addr.sun_path),
            "%.*s", static_cast<int>(path.size()), path.data());
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        return RawErrorConvert::fromErrno(errno);
    }

    if (aIsServer) {
        if (!isAbstract) {
            unlink(path.data());
        }

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            int e = errno;
            close(fd);
            return RawErrorConvert::fromErrno(e);
        }

        if (listen(fd, SOMAXCONN) < 0) {
            int e = errno;
            close(fd);
            unlink(path.data());
            return RawErrorConvert::fromErrno(e);
        }

        if (aListenFd) {
            *aListenFd = fd;
        }

        aBus = nullptr;
        return Status(StatusCode::SUCCESS);
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        int e = errno;
        close(fd);
        return RawErrorConvert::fromErrno(e);
    }

    Status st = RawErrorConvert::makeStatus(sd_bus_new(&aBus));
    if (st.isError()) {
        close(fd);
        return st;
    }

    st = RawErrorConvert::makeStatus(sd_bus_set_fd(aBus, fd, fd));
    if (st.isError()) {
        close(fd);
        unrefBus(aBus);
        return st;
    }

    st = RawErrorConvert::makeStatus(sd_bus_start(aBus));
    if (st.isError()) {
        close(fd);
        unrefBus(aBus);
        return st;
    }

    return Status(StatusCode::SUCCESS);
}

Status acceptConnection(RawBusPtr& aBus, int& aListenFd, RawBusEventPtr aEvent) {
    if (aListenFd < 0) {
        return Status(StatusCode::INVALID_ARG);
    }

    //! Close fd
    int fd = accept4(aListenFd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fd < 0) {
        return RawErrorConvert::fromErrno(errno);
    }

    //! Close litening fd
    close(aListenFd);
    aListenFd = -1;

    RawBusPtr bus = nullptr;
    int r = sd_bus_new(&bus);
    if (r < 0) {
        close(fd);
        return RawErrorConvert::fromErrno(-r);
    }

    sd_id128_t id;
    r = sd_id128_randomize(&id);
    if (r < 0) {
        close(fd);
        unrefBus(bus);
        return RawErrorConvert::fromErrno(-r);
    }

    r = sd_bus_set_server(bus, 1, id);
    if (r < 0) {
        close(fd);
        unrefBus(bus);
        return RawErrorConvert::fromErrno(-r);
    }

    r = sd_bus_set_fd(bus, fd, fd);
    if (r < 0) {
        close(fd);
        unrefBus(bus);
        return RawErrorConvert::fromErrno(-r);
    }

    r = sd_bus_start(bus);
    if (r < 0) {
        close(fd);
        unrefBus(bus);
        return RawErrorConvert::fromErrno(-r);
    }

    r = sd_bus_attach_event(bus, aEvent, 0);
    if (r < 0) {
        unrefBus(bus);
        return RawErrorConvert::fromErrno(-r);
    }

    aBus = bus;
    return Status(StatusCode::SUCCESS);
}

Status flushBus(RawBusPtr aBus) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_flush(aBus));
}

Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg) {
    if (!aBus || !aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_send(aBus, aMsg, nullptr));
}

Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg, std::string_view aDest) {
    if (!aBus || !aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_send_to(aBus, aMsg, aDest.data(), nullptr));
}

Status callSync(RawBusPtr aBus, RawBusMessagePtr aMsg,
    uint64_t aTimeoutUmsc, RawBusErrorPtr aErr, RawBusMessagePtr& aRep) {
    if (!aBus || !aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_call(aBus, aMsg, aTimeoutUmsc, aErr, &aRep));
}

Status callAsync(RawBusPtr aBus, RawBusSlotPtr& aSlot, RawBusMessagePtr aMsg,
    RawBusMessageHandler aCallback, void* aUsrData, uint64_t aTimeoutUmsc) {
    if (!aBus || !aMsg) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_call_async(
        aBus, &aSlot, aMsg, aCallback, aUsrData, aTimeoutUmsc
    ));
}

void closeBus(RawBusPtr aBus) {
    if (!aBus) {
        return;
    }

    if (isBusReady(aBus)) {
        flushBus(aBus);
    }

    if (isBusOpen(aBus)) {
        sd_bus_close(aBus);
    }
}

int getFd(RawBusPtr aBus) {
    if (!aBus) {
        return -1;
    }
    
    return sd_bus_get_fd(aBus);
}

int process(RawBusPtr aBus, RawBusMessagePtr* aMsg) {
    if (!aBus) {
        return -1;
    }

    return sd_bus_process(aBus, aMsg);
}

int wait(RawBusPtr aBus, uint64_t aTimeout) {
    if (!aBus) {
        return -1;
    }

    return sd_bus_wait(aBus, aTimeout);
}

Status attachEvent(RawBusPtr aBus, RawBusEventPtr aEvent, int aPrio) {
    if (!aBus || ! aEvent) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_attach_event(aBus, aEvent, aPrio));
}

Status detachEvent(RawBusPtr aBus) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_detach_event(aBus));
}

std::string getUniqueName(RawBusPtr aBus) {
    if (!aBus) {
        return "";
    }

    const char* name = nullptr;
    sd_bus_get_unique_name(aBus, &name);
    return name ? std::string(name) : std::string();
}

Status setUniqueName(RawBusPtr aBus, std::string_view aName, uint64_t aFlags) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_request_name(
        aBus, aName.data(), aFlags));
}

int PeerSignalFilter::onMessage(sd_bus_message* aMsg, void* aData, sd_bus_error*) {
    auto* filter = static_cast<PeerSignalFilter*>(aData);
    //! If signal
    if (!sd_bus_message_is_signal(aMsg,
        filter->iface.empty() ? nullptr : filter->iface.c_str(),
        filter->signal.empty() ? nullptr : filter->signal.c_str())) {
        return 0;
    }

    //! If from path
    const char* pth = sd_bus_message_get_path(aMsg);
    if (!filter->path.empty()
        && (!pth || filter->path != pth)) {
        return 0;
    }

    return filter->callback(aMsg, filter->data, nullptr);
}

Status listenSignal(RawBusPtr aBus, RawBusSlotPtr& aSlot, std::string_view aSender,
    std::string_view aPath, std::string_view aIface, std::string_view aSignal,
    RawBusMessageHandler aCallback, void* aData, bool aIsPeer) {
    int ret = 0;
    if (aIsPeer) {
        auto* ctx = new PeerSignalFilter{
            aPath.data(), aIface.data(), aSignal.data(),
            aCallback, aData};

        ret = sd_bus_add_filter(aBus, &aSlot,
            PeerSignalFilter::onMessage, ctx);
        if (ret >= 0) {
            sd_bus_slot_set_destroy_callback(aSlot, PeerSignalFilter::destroy);
            return Status(StatusCode::SUCCESS);
        }

        delete ctx;
    } else {
        int ret = sd_bus_match_signal(
            aBus, &aSlot,
            aSender.empty() ? nullptr : aSender.data(),
            aPath.empty() ? nullptr : aPath.data(),
            aIface.empty() ? nullptr : aIface.data(),
            aSignal.empty() ? nullptr : aSignal.data(),
            aCallback, aData);
        if (ret >= 0) {
            return Status(StatusCode::SUCCESS);
        }
    }

    return RawErrorConvert::makeStatus(ret);
}

Status addObjectToVTable(RawBusPtr aBus, RawBusSlotPtr& aSlot,
    std::string_view aPath, std::string_view aIface,
    Adaptor::RawBusVTable* aVTable, void* aData) {
    if (!aBus
        || !RawCheck::isPathNameValid(aPath)
        || !RawCheck::isInterfaceNameValid(aIface)
        || !aVTable) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_add_object_vtable(aBus, &aSlot, aPath.data(), aIface.data(), aVTable, aData)
    );
}

Status setWatchBind(RawBusPtr aBus, bool aIsEnabled) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_set_watch_bind(aBus, aIsEnabled)
    );
}

Status setExitOnDisconnect(RawBusPtr aBus, bool aIsEnabled) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_set_exit_on_disconnect(aBus, aIsEnabled)
    );
}

Status setConnectedSignal(RawBusPtr aBus, bool aIsEnabled) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_set_connected_signal(aBus, aIsEnabled)
    );
}

}

RawBusSharePtr::RawBusSharePtr(RawBusPtr aRawBusPtr, bool aIsOwned)
    : mRaw(aRawBusPtr)
    , mIsOwned(aIsOwned) {}

RawBusSharePtr::~RawBusSharePtr() {
    if (mIsOwned && mRaw) {
        RawBus::unrefBus(mRaw);
    }
    mRaw = nullptr;
}

RawBusSharePtr::RawBusSharePtr(const RawBusSharePtr& aPtr)
    : mRaw(aPtr.mRaw)
    , mIsOwned(aPtr.mIsOwned) {
    if (mRaw && mIsOwned) {
        RawBus::refBus(mRaw);
    }
}

RawBusSharePtr::RawBusSharePtr(RawBusSharePtr&& aPtr) noexcept
    : mRaw(aPtr.mRaw)
    , mIsOwned(aPtr.mIsOwned) {
    aPtr.mRaw = nullptr;
    aPtr.mIsOwned = false;
}

RawBusSharePtr& RawBusSharePtr::operator=(const RawBusSharePtr& aPtr) {
    if (this == &aPtr) {
        return *this;
    }

    if (mIsOwned && mRaw) {
        RawBus::unrefBus(mRaw);
    }

    mRaw = aPtr.mRaw;
    mIsOwned = aPtr.mIsOwned;
    if (mIsOwned && mRaw) {
        RawBus::refBus(mRaw);
    }

    return *this;
}

RawBusSharePtr& RawBusSharePtr::operator=(RawBusSharePtr&& aPtr) noexcept {
    if (this == &aPtr) {
        return *this;
    }

    if (mIsOwned && mRaw) {
        RawBus::unrefBus(mRaw);
    }

    mRaw = aPtr.mRaw;
    mIsOwned = aPtr.mIsOwned;
    aPtr.mRaw = nullptr;
    aPtr.mIsOwned = false;

    return *this;
}

RawBusSharePtr RawBusSharePtr::makeSystem() {
    RawBusPtr raw = nullptr;
    if (RawBus::openSystemBus(raw).isError()) {
        return RawBusSharePtr(nullptr, false);
    }

    return RawBusSharePtr(raw, true);
}

RawBusSharePtr RawBusSharePtr::makeUser() {
    RawBusPtr raw = nullptr;
    if (RawBus::openUserBus(raw).isError()) {
        return RawBusSharePtr(nullptr, false);
    }

    return RawBusSharePtr(raw, true);
}

RawBusSharePtr RawBusSharePtr::makePeer(std::string_view aSocket,
    bool aIsServer, int* aListenFd) {
    RawBusPtr raw = nullptr;
    if (aSocket == "" || RawBus::openPeerBus(
        raw, aSocket, aIsServer, aListenFd).isError()) {
        return RawBusSharePtr(nullptr, false);
    }

    return RawBusSharePtr(raw, true);
}
}
}