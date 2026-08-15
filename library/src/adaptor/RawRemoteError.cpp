#include "adaptor/RawRemoteError.hpp"


namespace SSDbus {
namespace Adaptor {
static const std::map<std::string_view, StatusCode> TO_MAP = {
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

static const std::map<StatusCode, const char*> FROM_MAP = {
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

RawRemoteError::RawRemoteError(RawBusErrorPtr aRawError) {
    sd_bus_error_copy(&mRawError, aRawError);
}

RawRemoteError::RawRemoteError()
    : mRawError(SD_BUS_ERROR_NULL) {}

RawRemoteError::~RawRemoteError() {
    if (sd_bus_error_is_set(&mRawError)) {
        sd_bus_error_free(&mRawError);
    }
}

RawRemoteError::RawRemoteError(const RawRemoteError& aOther) {
    sd_bus_error_copy(&mRawError, &aOther.mRawError);
}

RawRemoteError& RawRemoteError::operator=(const RawRemoteError& aOther) {
    if (this == &aOther) {
        return *this;
    }

    if (sd_bus_error_is_set(&mRawError)) {
        sd_bus_error_free(&mRawError);
    }

    sd_bus_error_copy(&mRawError, &aOther.mRawError);
    return *this;
}

RawRemoteError::RawRemoteError(RawRemoteError&& aOther) {
    sd_bus_error_move(&mRawError, &aOther.mRawError);
}

RawRemoteError& RawRemoteError::operator=(RawRemoteError&& aOther) {
    if (this == &aOther) {
        return *this;
    }

    if (sd_bus_error_is_set(&mRawError)) {
        sd_bus_error_free(&mRawError);
    }

    sd_bus_error_move(&mRawError, &aOther.mRawError);
    return *this;
}

Status RawRemoteError::toStatus() const {
    if (!mRawError.name) {
        return Status(StatusCode::UNKNOWN_ERROR);
    }

    auto it = TO_MAP.find(mRawError.name);
    if (it != TO_MAP.end()) {
        return Status(it->second);
    }

    return Status(StatusCode::UNKNOWN_ERROR);
}

RawRemoteError RawRemoteError::fromStatus(StatusCode aCode) {
    RawRemoteError err;
    auto it = FROM_MAP.find(aCode);
    const char* name = (it != FROM_MAP.end())
        ? it->second : SD_BUS_ERROR_FAILED;
    sd_bus_error_set(&err.mRawError, name, statusMessage(aCode));
    return err;
}

}
}