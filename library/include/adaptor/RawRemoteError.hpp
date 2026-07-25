
#ifndef SSDBUS_DBUS_ERROR_HPP
#define SSDBUS_DBUS_ERROR_HPP

#include <map>
#include <string_view>
#include "adaptor/RawCommon.hpp"

namespace SSDbus {
namespace Adaptor {

namespace RawErrorConvert {
inline StatusCode fromErrno(int aErrno) {
    switch (aErrno) {
        case 0:
            return StatusCode::SUCCESS;
        case EINVAL:
            return StatusCode::INVALID_ARG;
        case ENOENT:
            return StatusCode::NOT_FOUND;
        case EHOSTUNREACH:
            return StatusCode::NO_SERVICE;
        case EBADR:
            return StatusCode::NO_METHOD;
        case EACCES:
            return StatusCode::ACCESS_DENIED;
        case EADDRINUSE:
        case EEXIST:
            return StatusCode::NAME_EXISTS;
        case ENOTCONN:
            return StatusCode::NOT_CONNECTED;
        case ECONNRESET:
            return StatusCode::CONN_RESET;
        case EBUSY:
            return StatusCode::BUSY;
        case ETIMEDOUT:
            return StatusCode::TIMEOUT;
        case ENOMEM:
            return StatusCode::NO_MEMORY;
        case ENOMSG:
            return StatusCode::NO_REPLY;
        case EBADMSG:
            return StatusCode::PROTOCOL_ERROR;
        case EIO:
            return StatusCode::IO_ERROR;
        case EMSGSIZE:
            return StatusCode::MSG_TOO_LONG;
        case E2BIG:
            return StatusCode::LIMIT_EXCEEDED;
        case EPROTO:
            return StatusCode::PROTOCOL_ERROR;
        case ENOTSUP:
            return StatusCode::PROTOCOL_ERROR;
        case ENXIO:
            return StatusCode::TYPE_MISMATCH;
        default:
            return StatusCode::UNKNOWN_ERROR;
    }
}

inline Status makeStatus(int aRet) {
    if (aRet >= 0) {
        return Status(StatusCode::SUCCESS);
    }

    std::cerr << "[DEBUG] makeStatus ret=" << aRet << " errno=" << (-aRet) << std::endl;

    return Status(RawErrorConvert::fromErrno(-aRet));
}
}

class RawRemoteError {
public:
    explicit RawRemoteError(RawBusErrorPtr aRawError) {
        sd_bus_error_copy(&mRawError, aRawError);
    }

    RawRemoteError() : mRawError(SD_BUS_ERROR_NULL) {}

    ~RawRemoteError() {
        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }
    }

    RawRemoteError(const RawRemoteError& aOther) {
        sd_bus_error_copy(&mRawError, &aOther.mRawError);
    }

    RawRemoteError& operator=(const RawRemoteError& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }

        sd_bus_error_copy(&mRawError, &aOther.mRawError);
        return *this;
    }

    RawRemoteError(RawRemoteError&& aOther) {
        sd_bus_error_move(&mRawError, &aOther.mRawError);
    }

    RawRemoteError& operator=(RawRemoteError&& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (sd_bus_error_is_set(&mRawError)) {
            sd_bus_error_free(&mRawError);
        }

        sd_bus_error_move(&mRawError, &aOther.mRawError);
        return *this;
    }

    Status toStatus() const {
        static const std::map<std::string_view, StatusCode> kMap = {
            // --- common ---
            {SD_BUS_ERROR_FAILED,             StatusCode::UNKNOWN_ERROR},
            {SD_BUS_ERROR_NO_MEMORY,          StatusCode::NO_MEMORY},
            {SD_BUS_ERROR_NOT_SUPPORTED,      StatusCode::NO_METHOD},
            {SD_BUS_ERROR_LIMITS_EXCEEDED,    StatusCode::LIMIT_EXCEEDED},
            {SD_BUS_ERROR_INVALID_ARGS,       StatusCode::INVALID_ARG},
            {SD_BUS_ERROR_INVALID_SIGNATURE,  StatusCode::INVALID_ARG},
            {SD_BUS_ERROR_INCONSISTENT_MESSAGE, StatusCode::PROTOCOL_ERROR},

            //! --- service/object/method ---
            {SD_BUS_ERROR_SERVICE_UNKNOWN,    StatusCode::NO_SERVICE},
            {SD_BUS_ERROR_NAME_HAS_NO_OWNER,  StatusCode::NO_SERVICE},
            {SD_BUS_ERROR_UNKNOWN_METHOD,     StatusCode::NO_METHOD},
            {SD_BUS_ERROR_UNKNOWN_OBJECT,     StatusCode::NOT_FOUND},
            {SD_BUS_ERROR_UNKNOWN_INTERFACE,  StatusCode::NOT_FOUND},
            {SD_BUS_ERROR_UNKNOWN_PROPERTY,   StatusCode::NOT_FOUND},

            //! --- permission ---
            {SD_BUS_ERROR_ACCESS_DENIED,      StatusCode::ACCESS_DENIED},
            {SD_BUS_ERROR_AUTH_FAILED,        StatusCode::ACCESS_DENIED},
            {SD_BUS_ERROR_PROPERTY_READ_ONLY, StatusCode::ACCESS_DENIED},
            {SD_BUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED, StatusCode::ACCESS_DENIED},

            // --- connect/transition ---
            {SD_BUS_ERROR_TIMEOUT,            StatusCode::TIMEOUT},
            {SD_BUS_ERROR_NO_REPLY,           StatusCode::NO_REPLY},
            {SD_BUS_ERROR_IO_ERROR,           StatusCode::IO_ERROR},
            {SD_BUS_ERROR_DISCONNECTED,       StatusCode::DISCONNECTED},
            {SD_BUS_ERROR_BAD_ADDRESS,        StatusCode::INVALID_ARG},

            // --- file ---
            {SD_BUS_ERROR_FILE_NOT_FOUND,     StatusCode::NOT_FOUND},
            {SD_BUS_ERROR_FILE_EXISTS,        StatusCode::NAME_EXISTS},

            // --- match rule ---
            {SD_BUS_ERROR_MATCH_RULE_NOT_FOUND, StatusCode::NOT_FOUND},
            {SD_BUS_ERROR_MATCH_RULE_INVALID,   StatusCode::INVALID_ARG},
        };

        auto it = kMap.find(mRawError.name);
        if (it != kMap.end()) {
            return Status(it->second);
        }

        return Status(StatusCode::UNKNOWN_ERROR);
    }

    static RawRemoteError fromStatus(StatusCode aCode) {
        static const std::map<StatusCode, const char*> kMap = {
            {StatusCode::INVALID_ARG,    SD_BUS_ERROR_INVALID_ARGS},
            {StatusCode::NOT_FOUND,      SD_BUS_ERROR_UNKNOWN_OBJECT},
            {StatusCode::NO_SERVICE,     SD_BUS_ERROR_SERVICE_UNKNOWN},
            {StatusCode::NO_METHOD,      SD_BUS_ERROR_UNKNOWN_METHOD},
            {StatusCode::ACCESS_DENIED,  SD_BUS_ERROR_ACCESS_DENIED},
            {StatusCode::NAME_EXISTS,    SD_BUS_ERROR_FILE_EXISTS},
            {StatusCode::NOT_CONNECTED,  SD_BUS_ERROR_DISCONNECTED},
            {StatusCode::CONN_RESET,     SD_BUS_ERROR_DISCONNECTED},
            {StatusCode::BUSY,           SD_BUS_ERROR_LIMITS_EXCEEDED},
            {StatusCode::TIMEOUT,        SD_BUS_ERROR_TIMEOUT},
            {StatusCode::NO_MEMORY,      SD_BUS_ERROR_NO_MEMORY},
            {StatusCode::NO_REPLY,       SD_BUS_ERROR_NO_REPLY},
            {StatusCode::IO_ERROR,       SD_BUS_ERROR_IO_ERROR},
            {StatusCode::MSG_TOO_LONG,   SD_BUS_ERROR_LIMITS_EXCEEDED},
            {StatusCode::LIMIT_EXCEEDED, SD_BUS_ERROR_LIMITS_EXCEEDED},
            {StatusCode::PROTOCOL_ERROR, SD_BUS_ERROR_INCONSISTENT_MESSAGE},
            {StatusCode::TYPE_MISMATCH,  SD_BUS_ERROR_INVALID_ARGS},
            {StatusCode::DISCONNECTED,   SD_BUS_ERROR_DISCONNECTED},
            {StatusCode::UNKNOWN_ERROR,  SD_BUS_ERROR_FAILED},
        };

        RawRemoteError err;
        auto it = kMap.find(aCode);
        const char* name = (it != kMap.end()) ? it->second : SD_BUS_ERROR_FAILED;
        sd_bus_error_set(&err.mRawError, name, statusMessage(aCode));
        return err;
    }

    RawBusErrorPtr getRawPtr() {
        return &mRawError;
    }

    void moveTo(RawBusErrorPtr& aPtr) {
        //! The return values of sd_bus_error_move/copy are -errno (reflecting the
        //! error type, e.g. -EINVAL for SD_BUS_ERROR_INVALID_ARGS), not success/failure.
        //! Since the caller cannot distinguish "operation failed" from "error type is EINVAL",
        //! we simply ignore the return and rely on the side effect (dest is modified in-place).
        sd_bus_error_move(aPtr, &mRawError);
    }

    void copyTo(RawBusErrorPtr& aPtr) {
        //! sd_bus_error_copy has the same return convention: -errno, not success/failure.
        //! Same reason as moveTo — we ignore the return.
        sd_bus_error_copy(aPtr, &mRawError);
    }

private:
    RawBusError mRawError;
};
}
}

#endif