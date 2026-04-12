
#ifndef SSDBUS_DBUS_ERROR_HPP
#define SSDBUS_DBUS_ERROR_HPP

#include <ostream>
#include <systemd/sd-bus.h>
#include <string>
#include "RawAdaptor.hpp"

namespace SSDbus {

class DbusError {
public:
    explicit DbusError(sd_bus_error* aRawError) {
        sd_bus_error_move(&mRawError, aRawError);
    }

    explicit DbusError(const std::string&& aErrName, const std::string&& aErrMsg)
        : mRawError(SD_BUS_ERROR_NULL) {
        int ret = sd_bus_error_set(&mRawError, aErrName.c_str(), aErrMsg.c_str());
        if (ret < 0) {
            throw DbusException("Failed to create bus error: ", strerror(-ret));
        }
    }

    explicit DbusError(const char* aErrName, const char* aErrMsg)
        : mRawError(SD_BUS_ERROR_NULL) {
        int ret = sd_bus_error_set(&mRawError, aErrName, aErrMsg);
        if (ret < 0) {
            throw DbusException("Failed to create bus error: ", strerror(-ret));
        }
    }

    explicit DbusError(int aErrno)
        : mRawError(SD_BUS_ERROR_NULL) {
        int ret = sd_bus_error_set_errno(&mRawError, aErrno);
        if (ret < 0) {
            throw DbusException("Failed to create bus error: ", strerror(-ret));
        }
    }

    DbusError() : mRawError(SD_BUS_ERROR_NULL) {}

    ~DbusError() {
        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }
    }

    DbusError(const DbusError& aOther) {
        sd_bus_error_copy(&mRawError, &aOther.mRawError);
    }

    DbusError& operator=(const DbusError& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }

        sd_bus_error_copy(&mRawError, &aOther.mRawError);
        return *this;
    }

    DbusError(DbusError&& aOther) {
        sd_bus_error_move(&mRawError, &aOther.mRawError);
    }

    DbusError& operator=(DbusError&& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }

        sd_bus_error_move(&mRawError, &aOther.mRawError);
        return *this;
    }

    static DbusError createError(std::string& aName, const std::string& aMsg) {
        DbusError error;
        sd_bus_error_set(error.getRawPtr(), aName.c_str(), aMsg.c_str());
        return error;
    }

    sd_bus_error* getRawPtr() {
        return &mRawError;
    }

    std::string errorString() const {
        if (!sd_bus_error_is_set(&mRawError)) {
            return "";
        }

        return std::string(mRawError.name).
            append(": ").append(mRawError.message);
    }

private:
    sd_bus_error mRawError;
};

std::ostream& operator<<(std::ostream& aOstream, const DbusError& aErr) {
    const std::string str = aErr.errorString();
    return aOstream << str;
}

}

#endif