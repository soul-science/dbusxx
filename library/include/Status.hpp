
#ifndef DBUSXX_DBUS_RETURN_STATUS_HPP
#define DBUSXX_DBUS_RETURN_STATUS_HPP

#include <cstdint>
#include <string>


namespace Dbusxx {
/**
 * @brief Result codes returned by D-Bus operations.
 *
 * Values are grouped into categories: caller errors, connection errors,
 * transport errors, D-Bus protocol errors and an unknown fallback.
 */
enum class StatusCode : uint8_t {
    SUCCESS = 0,

    //! --- Caller errors ---
    INVALID_ARG,        //!< Invalid argument
    NOT_FOUND,          //!< Service/object/interface not found
    NO_SERVICE,         //!< Service not found
    NO_METHOD,          //!< Method not found (path/interface/method error)
    ACCESS_DENIED,      //!< Insufficient permission
    NAME_EXISTS,        //!< Bus name already taken

    //! --- Connection errors ---
    NOT_CONNECTED,      //!< Not connected to the bus
    CONN_RESET,         //!< Connection reset
    BUSY,               //!< Resource busy

    //! --- Transport errors ---
    TIMEOUT,            //!< Call timed out
    NO_MEMORY,          //!< Out of memory
    NO_REPLY,           //!< No reply received
    IO_ERROR,           //!< I/O error
    MSG_TOO_LONG,       //!< Message too long
    LIMIT_EXCEEDED,     //!< Limit exceeded

    //! --- D-Bus protocol errors ---
    PROTOCOL_ERROR,     //!< Protocol error
    TYPE_MISMATCH,      //!< Type mismatch
    DISCONNECTED,       //!< Peer disconnected

    //! --- Unknown ---
    UNKNOWN_ERROR       //!< Unknown error (fallback)
};

/**
 * @brief Convert a status code into a human-readable message string.
 *
 * @param aCode status code
 * @return message string
 */
inline constexpr const char* statusMessage(StatusCode aCode) {
    switch (aCode) {
        case StatusCode::SUCCESS:
            return "Success";
        case StatusCode::INVALID_ARG:
            return "Invalid argument";
        case StatusCode::NOT_FOUND:
            return "Service/object not found";
        case StatusCode::NO_SERVICE:
            return "Service not found";
        case StatusCode::NO_METHOD:
            return "Method not found, maybe path/interface/method error";
        case StatusCode::ACCESS_DENIED:
            return "Access denied";
        case StatusCode::NAME_EXISTS:
            return "name already taken";
        case StatusCode::NOT_CONNECTED:
            return "Not connected to bus";
        case StatusCode::CONN_RESET:
            return "Connection reset";
        case StatusCode::BUSY:
            return "Resource busy";
        case StatusCode::TIMEOUT:
            return "Operation timed out";
        case StatusCode::NO_MEMORY:
            return "Out of memory";
        case StatusCode::NO_REPLY:
            return "No reply received";
        case StatusCode::IO_ERROR:
            return "I/O error";
        case StatusCode::MSG_TOO_LONG:
            return "Message too long";
        case StatusCode::LIMIT_EXCEEDED:
            return "Limit exceeded";
        case StatusCode::PROTOCOL_ERROR:
            return "Protocol error";
        case StatusCode::DISCONNECTED:
            return "Peer disconnected";
        case StatusCode::TYPE_MISMATCH:
            return "Type mismatch";
        case StatusCode::UNKNOWN_ERROR:
            return "Unknown error";
    }

    return "Unknown";
}

/**
 * @brief Lightweight wrapper around a #StatusCode.
 *
 * Use `isSuccess()`/`isError()` to inspect the result instead of
 * relying on implicit boolean conversion.
 */
class Status {
public:
    //! @brief Construct a successful status by default.
    Status() = default;

    /**
     * @brief Construct a status from the given code.
     *
     * @param aCode status code
     */
    Status(StatusCode aCode)
        : mCode(aCode) {}

    //! @brief Return the underlying result code.
    [[nodiscard]] inline StatusCode code() const {
        return mCode;
    }

    //! @brief Return true if the operation succeeded.
    [[nodiscard]] inline bool isSuccess() const {
        return mCode == StatusCode::SUCCESS;
    
    }

    //! @brief Return true if the operation failed.
    [[nodiscard]] inline bool isError() const {
        return mCode != StatusCode::SUCCESS;
    }

    //! @brief Return a human-readable description of the result.
    [[nodiscard]] inline std::string message() const {
        return statusMessage(mCode);
    }

private:
    StatusCode mCode { StatusCode::SUCCESS };
};

}

#endif