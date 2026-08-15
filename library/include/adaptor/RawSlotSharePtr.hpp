#ifndef SSDBUS_RAW_SLOT_SHARE_PTR_HPP
#define SSDBUS_RAW_SLOT_SHARE_PTR_HPP

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawRemoteError.hpp"

namespace SSDbus {
namespace Adaptor {
namespace RawSlot {
void unrefSlot(RawBusSlotPtr aSlot);

void refSlot(RawBusSlotPtr aSlot);

RawBusPtr getBus(RawBusSlotPtr aSlot);

void* getUserdata(RawBusSlotPtr aSlot);

void setUserdata(RawBusSlotPtr aSlot, void* aUserdata);

Status setSlotDeleterCallback(RawBusSlotPtr aSlot, RawDeleterCallback aCallback);

RawDeleterCallback getSlotDeleterCallback(RawBusSlotPtr aSlot);
}

class RawSlotSharePtr {
public:
    RawSlotSharePtr() = default;

    explicit RawSlotSharePtr(Adaptor::RawBusSlotPtr aRawSolt);

    ~RawSlotSharePtr();

    RawSlotSharePtr(const RawSlotSharePtr& aOther);

    RawSlotSharePtr(RawSlotSharePtr&& aOther) noexcept;

    RawSlotSharePtr& operator=(const RawSlotSharePtr& aOther);

    RawSlotSharePtr& operator=(RawSlotSharePtr&& aOther) noexcept;

private:
    Adaptor::RawBusSlotPtr mRawSlot { nullptr };
};

}
}


#endif