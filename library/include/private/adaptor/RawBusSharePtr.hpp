#ifndef SSDBUS_RAW_BUS_SHARE_PTR_HPP
#define SSDBUS_RAW_BUS_SHARE_PTR_HPP

#include "private/adaptor/RawCommon.hpp"

#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>


namespace SSDbus {
namespace Adaptor {
namespace RawBus {
void unrefBus(RawBusPtr aBus);

void refBus(RawBusPtr aBus);

bool isBusReady(RawBusPtr aBus);

bool isBusOpen(RawBusPtr aBus);

Status openSystemBus(RawBusPtr& aBus);

Status openUserBus(RawBusPtr& aBus);

Status openPeerBus(RawBusPtr& aBus, std::string_view aSocket,
    bool aIsServer, int* aListenFd = nullptr);

Status acceptConnection(RawBusPtr& aBus, int& aListenFd, RawBusEventPtr aEvent);

Status flushBus(RawBusPtr aBus);

Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg);

Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg, std::string_view aDest);

Status callSync(RawBusPtr aBus, RawBusMessagePtr aMsg,
    uint64_t aTimeoutUmsc, RawBusErrorPtr aErr, RawBusMessagePtr& aRep);

Status callAsync(RawBusPtr aBus, RawBusSlotPtr& aSlot, RawBusMessagePtr aMsg,
    RawBusMessageHandler aCallback, void* aUsrData, uint64_t aTimeoutUmsc);

void closeBus(RawBusPtr aBus);

int getFd(RawBusPtr aBus);

int process(RawBusPtr aBus, RawBusMessagePtr* aMsg);

int wait(RawBusPtr aBus, uint64_t aTimeout);

Status attachEvent(RawBusPtr aBus, RawBusEventPtr aEvent, int aPrio);

Status detachEvent(RawBusPtr aBus);

std::string getUniqueName(RawBusPtr aBus);

Status setUniqueName(RawBusPtr aBus, std::string_view aName, uint64_t aFlags);

Status listenSignal(RawBusPtr aBus, RawBusSlotPtr& aSlot, std::string_view aSender,
    std::string_view aPath, std::string_view aIface, std::string_view aSignal,
    RawBusMessageHandler aCallback, void* aData, bool aIsPeer = false);

Status addObjectToVTable(RawBusPtr aBus, RawBusSlotPtr& aSlot,
    std::string_view aPath, std::string_view aIface,
    Adaptor::RawBusVTable* aVTable, void* aData);

//! Set if watch bus_daemon
Status setWatchBind(RawBusPtr aBus, bool aIsEnabled);

//! Set auto close dbus when bus_daemon disconnect 
Status setExitOnDisconnect(RawBusPtr aBus, bool aIsEnabled);

Status setConnectedSignal(RawBusPtr aBus, bool aIsEnabled);
};

class RawBusSharePtr {
public:
    explicit RawBusSharePtr(RawBusPtr aRawBusPtr, bool aIsOwned = false);

    RawBusSharePtr(const RawBusSharePtr& aPtr);

    ~RawBusSharePtr();

    RawBusSharePtr(RawBusSharePtr&& aPtr) noexcept;

    RawBusSharePtr& operator=(const RawBusSharePtr& aPtr);

    RawBusSharePtr& operator=(RawBusSharePtr&& aPtr) noexcept;

    explicit inline operator bool() const {
        return mRaw != nullptr;
    }

    inline RawBusPtr get() const {
        return mRaw;
    }

    inline bool isOwned() const {
        return mIsOwned;
    }

    static RawBusSharePtr makeSystem();

    static RawBusSharePtr makeUser();

    static RawBusSharePtr makePeer(std::string_view aSocket,
        bool aIsServer = false, int* aListenFd = nullptr);

private:
    RawBusPtr mRaw { nullptr };
    bool mIsOwned { false };
};

}
}
#endif