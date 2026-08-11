#ifndef SSDBUS_RAW_BUS_SHARE_PTR_HPP
#define SSDBUS_RAW_BUS_SHARE_PTR_HPP

#include "adaptor/RawCommon.hpp"

#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

namespace SSDbus {
namespace Adaptor {

namespace RawBus {
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

Status openBus(RawBusPtr& aBus) {
    return RawErrorConvert::makeStatus(sd_bus_open(&aBus));
}

Status openSystemBus(RawBusPtr& aBus) {
    return RawErrorConvert::makeStatus(sd_bus_open_system(&aBus));
}

Status openUserBus(RawBusPtr& aBus) {
    return RawErrorConvert::makeStatus(sd_bus_open_user(&aBus));
}

Status openPeerBus(RawBusPtr& aBus,
    std::string_view aSocket, bool isServer) {
    Status st = RawErrorConvert::makeStatus(sd_bus_new(&aBus));
    if (st.isError()) {
        return st;
    }

    if (isServer) {
        sd_id128_t id;
        st = RawErrorConvert::makeStatus(sd_id128_randomize(&id));
        if (st.isError()) {
            unrefBus(aBus);
            return st;
        }

        st = RawErrorConvert::makeStatus(
            sd_bus_set_server(aBus, 1, id));
    } else {
        st = RawErrorConvert::makeStatus(sd_bus_set_bus_client(aBus, 0));
    }

    if (st.isError()) {
        unrefBus(aBus);
        return st;
    }

    std::string addr(aSocket);
    st = RawErrorConvert::makeStatus(sd_bus_set_address(aBus, addr.c_str()));
    if (st.isError()) {
        unrefBus(aBus);
        return st;
    }

    st = RawErrorConvert::makeStatus(sd_bus_start(aBus));
    if (st.isError()) {
    std::cerr << "[openPeerBus] sd_bus_start → ret=" << static_cast<int>(st.code())
                << ", messge=" << st.message() << std::endl;
        unrefBus(aBus);
        return st;
    }

    std::cerr << "isBusReady: " << isBusReady(aBus) << std::endl;

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

RawBusSlotPtr getCurSlot(RawBusPtr aBus) {
    if (!aBus) {
        return nullptr;
    }

    return sd_bus_get_current_slot(aBus);
}

RawBusMessagePtr getCurMessage(RawBusPtr aBus) {
    if (!aBus) {
        return nullptr;
    }

    return sd_bus_get_current_message(aBus);
}

RawBusMessageHandler getCurMessageHandler(RawBusPtr aBus) {
    if (!aBus) {
        return nullptr;
    }

    return sd_bus_get_current_handler(aBus);
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

RawBusEventPtr getCurEvent(RawBusPtr aBus) {
    if (!aBus) {
        return nullptr;
    }

    return sd_bus_get_event(aBus);
}

std::string_view getUniqueName(RawBusPtr aBus) {
    if (!aBus) {
        return "";
    }

    const char* name = nullptr;
    sd_bus_get_unique_name(aBus, &name);
    return name ? std::string_view(name) : std::string_view();
}

Status setUniqueName(RawBusPtr aBus, std::string_view aName, uint64_t aFlags) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(sd_bus_request_name(
        aBus, aName.data(), aFlags));
}

Status listenSignal(RawBusPtr aBus, RawBusSlotPtr& aSlot,
    std::string_view aSender, std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, RawBusMessageHandler aCallback, void* aData) {
    return RawErrorConvert::makeStatus(
        sd_bus_match_signal(
            aBus, &aSlot,
            aSender.data() != "" ? aSender.data() : nullptr,
            aPath.data() != "" ? aPath.data() : nullptr,
            aIface.data() != "" ? aIface.data() : nullptr,
            aSignal.data() != "" ? aSignal.data() : nullptr,
            aCallback, aData
        ));
}

Status addObjectToVTable(RawBusPtr aBus, RawBusSlotPtr& aSlot,
    std::string_view aPath, std::string_view aIface, Adaptor::RawBusVTable* aVTable, void* aData) {
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

//! Set if watch bus_daemon
Status setWatchBind(RawBusPtr aBus, bool aIsEnabled) {
    if (!aBus) {
        return Status(StatusCode::INVALID_ARG);
    }

    return RawErrorConvert::makeStatus(
        sd_bus_set_watch_bind(aBus, aIsEnabled)
    );
}

//! Set auto close dbus when bus_daemon disconnect 
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
};

class RawBusSharePtr {
public:
    explicit RawBusSharePtr(RawBusPtr aRawBusPtr, bool aIsOwned = false)
        : mRaw(aRawBusPtr)
        , mIsOwned(aIsOwned) {}

    ~RawBusSharePtr() {
        if (mIsOwned && mRaw) {
            RawBus::unrefBus(mRaw);
        }
        mRaw = nullptr;
    }

    RawBusSharePtr(const RawBusSharePtr& aPtr)
        : mRaw(aPtr.mRaw)
        , mIsOwned(aPtr.mIsOwned) {
        if (mRaw && mIsOwned) {
            RawBus::refBus(mRaw);
        }
    }

    RawBusSharePtr(RawBusSharePtr&& aPtr) noexcept
    : mRaw(aPtr.mRaw)
    , mIsOwned(aPtr.mIsOwned) {
        aPtr.mRaw = nullptr;
        aPtr.mIsOwned = false;
    }

    RawBusSharePtr& operator=(const RawBusSharePtr& aPtr) {
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

    RawBusSharePtr& operator=(RawBusSharePtr&& aPtr) noexcept {
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

    explicit operator bool() const {
        return mRaw != nullptr;
    }

    RawBusPtr get() const {
        return mRaw;
    }

    bool isOwned() const {
        return mIsOwned;
    }

    static RawBusSharePtr makeSystem() {
        RawBusPtr raw = nullptr;
        if (RawBus::openSystemBus(raw).isError()) {
            return RawBusSharePtr(nullptr, false);
        }

        return RawBusSharePtr(raw, true);
    }

    static RawBusSharePtr makeUser() {
        RawBusPtr raw = nullptr;
        if (RawBus::openUserBus(raw).isError()) {
            return RawBusSharePtr(nullptr, false);
        }

        return RawBusSharePtr(raw, true);
    }

    static RawBusSharePtr makePeer(std::string_view aSocket, bool aIsServer = false) {
        RawBusPtr raw = nullptr;
        if (aSocket == "" || RawBus::openPeerBus(
            raw, aSocket, aIsServer).isError()) {
            return RawBusSharePtr(nullptr, false);
        }

        return RawBusSharePtr(raw, true);
    }

private:
    RawBusPtr mRaw { nullptr };
    bool mIsOwned { false };
};

}
}
#endif