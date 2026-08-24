//! Unit tests for errno / sd_bus_error → StatusCode mapping
//! (library/include/private/adaptor/RawRemoteError.hpp).
//! Pure table lookups — no D-Bus daemon needed.

#include <gtest/gtest.h>

#include <cerrno>
#include <vector>

#include "private/adaptor/RawRemoteError.hpp"

using namespace Dbusxx;
using namespace Dbusxx::Adaptor;

TEST(RawErrorTest, FromErrnoCoverage) {
    //! 0 → SUCCESS
    EXPECT_EQ(RawErrorConvert::fromErrno(0), StatusCode::SUCCESS);

    //! caller errors
    EXPECT_EQ(RawErrorConvert::fromErrno(EINVAL), StatusCode::INVALID_ARG);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOENT), StatusCode::NOT_FOUND);
    EXPECT_EQ(RawErrorConvert::fromErrno(EHOSTUNREACH), StatusCode::NO_SERVICE);
    EXPECT_EQ(RawErrorConvert::fromErrno(EBADR), StatusCode::NO_METHOD);
    EXPECT_EQ(RawErrorConvert::fromErrno(EACCES), StatusCode::ACCESS_DENIED);
    EXPECT_EQ(RawErrorConvert::fromErrno(EADDRINUSE), StatusCode::NAME_EXISTS);
    EXPECT_EQ(RawErrorConvert::fromErrno(EEXIST), StatusCode::NAME_EXISTS);

    //! connection errors
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOTCONN), StatusCode::NOT_CONNECTED);
    EXPECT_EQ(RawErrorConvert::fromErrno(ECONNRESET), StatusCode::CONN_RESET);
    EXPECT_EQ(RawErrorConvert::fromErrno(EBUSY), StatusCode::BUSY);

    //! transport errors
    EXPECT_EQ(RawErrorConvert::fromErrno(ETIMEDOUT), StatusCode::TIMEOUT);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOMEM), StatusCode::NO_MEMORY);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOMSG), StatusCode::NO_REPLY);
    EXPECT_EQ(RawErrorConvert::fromErrno(EIO), StatusCode::IO_ERROR);
    EXPECT_EQ(RawErrorConvert::fromErrno(EMSGSIZE), StatusCode::MSG_TOO_LONG);
    EXPECT_EQ(RawErrorConvert::fromErrno(E2BIG), StatusCode::LIMIT_EXCEEDED);

    //! protocol errors
    EXPECT_EQ(RawErrorConvert::fromErrno(EBADMSG), StatusCode::PROTOCOL_ERROR);
    EXPECT_EQ(RawErrorConvert::fromErrno(EPROTO), StatusCode::PROTOCOL_ERROR);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOTSUP), StatusCode::PROTOCOL_ERROR);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENXIO), StatusCode::TYPE_MISMATCH);

    //! unknown fallback
    EXPECT_EQ(RawErrorConvert::fromErrno(9999), StatusCode::UNKNOWN_ERROR);
    EXPECT_EQ(RawErrorConvert::fromErrno(ENOTTY), StatusCode::UNKNOWN_ERROR);
}

TEST(RawErrorTest, MakeStatus) {
    EXPECT_TRUE(RawErrorConvert::makeStatus(0).isSuccess());
    EXPECT_TRUE(RawErrorConvert::makeStatus(42).isSuccess());  //! >= 0 → SUCCESS
    EXPECT_EQ(RawErrorConvert::makeStatus(-EINVAL).code(), StatusCode::INVALID_ARG);
    EXPECT_EQ(RawErrorConvert::makeStatus(-ETIMEDOUT).code(), StatusCode::TIMEOUT);
    EXPECT_EQ(RawErrorConvert::makeStatus(-12345).code(), StatusCode::UNKNOWN_ERROR);
}

TEST(RawErrorTest, FromStatusToStatusIdentity) {
    //! Codes with a 1:1 mapping survive fromStatus()→toStatus() unchanged.
    const std::vector<StatusCode> identity = {
        StatusCode::INVALID_ARG, StatusCode::NOT_FOUND, StatusCode::NO_SERVICE,
        StatusCode::NO_METHOD, StatusCode::ACCESS_DENIED, StatusCode::NAME_EXISTS,
        StatusCode::TIMEOUT, StatusCode::NO_MEMORY, StatusCode::NO_REPLY,
        StatusCode::IO_ERROR, StatusCode::LIMIT_EXCEEDED,
        StatusCode::PROTOCOL_ERROR, StatusCode::DISCONNECTED,
        StatusCode::UNKNOWN_ERROR,
    };
    for (StatusCode c : identity) {
        RawRemoteError err = RawRemoteError::fromStatus(c);
        EXPECT_EQ(err.toStatus().code(), c);
    }

    //! Codes that collapse onto a shared sd_bus_error name come back as the
    //! canonical target (many-to-one mapping — by design).
    EXPECT_EQ(RawRemoteError::fromStatus(StatusCode::NOT_CONNECTED)
                  .toStatus().code(),
              StatusCode::DISCONNECTED);
    EXPECT_EQ(RawRemoteError::fromStatus(StatusCode::CONN_RESET)
                  .toStatus().code(),
              StatusCode::DISCONNECTED);
    EXPECT_EQ(RawRemoteError::fromStatus(StatusCode::BUSY)
                  .toStatus().code(),
              StatusCode::LIMIT_EXCEEDED);
    EXPECT_EQ(RawRemoteError::fromStatus(StatusCode::MSG_TOO_LONG)
                  .toStatus().code(),
              StatusCode::LIMIT_EXCEEDED);
    EXPECT_EQ(RawRemoteError::fromStatus(StatusCode::TYPE_MISMATCH)
                  .toStatus().code(),
              StatusCode::INVALID_ARG);
}

TEST(RawErrorTest, EmptyErrorMapsToUnknown) {
    //! Unset error → UNKNOWN_ERROR.
    RawRemoteError err;  //! SD_BUS_ERROR_NULL
    EXPECT_EQ(err.toStatus().code(), StatusCode::UNKNOWN_ERROR);
}

TEST(RawErrorTest, RawSdBusErrorMessageMapping) {
    //! Construct a raw sd_bus_error and map it through RawRemoteError.
    sd_bus_error raw = SD_BUS_ERROR_NULL;
    sd_bus_error_set(&raw, SD_BUS_ERROR_SERVICE_UNKNOWN, "no such service");
    {
        RawRemoteError err(&raw);
        EXPECT_EQ(err.toStatus().code(), StatusCode::NO_SERVICE);
    }
    sd_bus_error_free(&raw);

    //! An unmapped error name → UNKNOWN_ERROR.
    //! Use a *fresh* sd_bus_error: sd_bus_error_set refuses to overwrite an
    //! already-set error (returns -EEXIST), so reusing `raw` above would
    //! leave the old name in place.
    sd_bus_error raw2 = SD_BUS_ERROR_NULL;
    sd_bus_error_set(&raw2, "com.example.CustomError", "custom");
    {
        RawRemoteError err(&raw2);
        EXPECT_EQ(err.toStatus().code(), StatusCode::UNKNOWN_ERROR);
    }
    sd_bus_error_free(&raw2);
}
