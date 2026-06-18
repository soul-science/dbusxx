
#ifndef SSDBUS_DBUS_RETURN_STATUS_HPP
#define SSDBUS_DBUS_RETURN_STATUS_HPP

#include <memory>

#include "DbusError.hpp"

namespace SSDbus {

class DbusReturnStatus {
public:
    enum class Status {
        SUCCESS = 0,
        FAIL
    };

    DbusReturnStatus() = default;

    explicit DbusReturnStatus(Status aStatus)
        : mStatus(aStatus) {}

    explicit DbusReturnStatus(Status aStatus, DbusError&& aErr)
        : mStatus(aStatus)
        , mErr(std::make_shared<DbusError>(std::forward<DbusError>(aErr))) {}

    void setStatus(Status aStatus) {
        mStatus = aStatus;
    }

    Status getStatus() const {
        return mStatus;
    }

    std::shared_ptr<DbusError> getError() const {
        return mErr;
    }

    explicit operator bool() const {
        return mStatus == Status::SUCCESS;
    }

private:
    Status mStatus { Status::SUCCESS };
    std::shared_ptr<DbusError> mErr { nullptr };
};

}

#endif