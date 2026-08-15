
#ifndef SSDBUS_DBUS_ERROR_HPP
#define SSDBUS_DBUS_ERROR_HPP

#include <map>
#include <string_view>
#include "adaptor/RawCommon.hpp"

namespace SSDbus {
namespace Adaptor {

namespace RawErrorConvert {
inline constexpr StatusCode fromErrno(int aErrno) {
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
    explicit RawRemoteError(RawBusErrorPtr aRawError);

    RawRemoteError();

    ~RawRemoteError();

    RawRemoteError(const RawRemoteError& aOther);

    RawRemoteError& operator=(const RawRemoteError& aOther);

    RawRemoteError(RawRemoteError&& aOther);

    RawRemoteError& operator=(RawRemoteError&& aOther);

    Status toStatus() const;

    static RawRemoteError fromStatus(StatusCode aCode);

    inline RawBusErrorPtr getRawPtr() {
        return &mRawError;
    }

    inline void moveTo(RawBusErrorPtr& aPtr) {
        //! The return values of sd_bus_error_move/copy are -errno (reflecting the
        //! error type, e.g. -EINVAL for SD_BUS_ERROR_INVALID_ARGS), not success/failure.
        //! Since the caller cannot distinguish "operation failed" from "error type is EINVAL",
        //! we simply ignore the return and rely on the side effect (dest is modified in-place).
        sd_bus_error_move(aPtr, &mRawError);
    }

    inline void copyTo(RawBusErrorPtr& aPtr) {
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