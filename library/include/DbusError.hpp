
#ifndef SSDBUS_DBUS_ERROR_HPP
#define SSDBUS_DBUS_ERROR_HPP

#include <ostream>
#include <systemd/sd-bus.h>
#include <string>
#include <stdexcept>

namespace SSDbus {

class DbusException : public std::runtime_error {
public:
    explicit DbusException(const std::string& aMsg)
        : std::runtime_error(aMsg) {}

    explicit DbusException(const std::string& aName, const std::string& msg)
        : std::runtime_error(aName + ": " + msg) {}
};

class DbusError {
public:
    explicit DbusError(sd_bus_error* aRawError) {
        sd_bus_error_move(&mRawError, aRawError);
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