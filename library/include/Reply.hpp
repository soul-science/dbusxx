#ifndef DBUSXX_DBUS_REPLY_HPP
#define DBUSXX_DBUS_REPLY_HPP

#include <string>
#include <utility>

#include "Message.hpp"


namespace Dbusxx {
/**
 * @brief A typed reply received from a remote method call.
 *
 * `Reply<Ret>` wraps a D-Bus reply message and provides a parsed value
 * of type `Ret`. Always check `isError()` (or `status()`) before reading
 * `value()`; on failure the value is a default-constructed `Ret`.
 */
template<typename Ret>
class Reply {
    static_assert(isValidArg<Ret>(), "Unsupported value type");
public:
    //! @brief Construct an empty reply.
    Reply() = default;

    /**
     * @brief Construct a reply from an implementation, parsing the payload.
     *
     * @param aImpl shared implementation to wrap
     */
    explicit Reply(std::shared_ptr<Private::MessagePrivate> aImpl)
        : mMessage(std::move(aImpl)) {
        mStatus = mMessage.read(mValue);
    }

    /**
     * @brief Construct a reply by moving in an implementation and parsing it.
     *
     * @param aImpl implementation to move from
     */
    explicit Reply(Private::MessagePrivate&& aImpl)
        : Reply(std::make_shared<Private::MessagePrivate>(std::move(aImpl))) {}

    //! @brief Return the parsed return value (only valid when `isError()` is false).
    [[nodiscard]] const Ret& value() const {
        return mValue;
    }

    //! @brief Return the overall status of the call/reply.
    [[nodiscard]] Status status() const {
        return mMessage.isError() ? mMessage.status() : mStatus;
    }

    //! @brief Return true if the reply indicates an error.
    [[nodiscard]] bool isError() const {
        return mStatus.isError() || mMessage.isError();
    }

    //! @brief Return the error description when `isError()` is true.
    [[nodiscard]] std::string errorMessage() const {
        return mMessage.isError() ? mMessage.errorMessage() : mStatus.message();
    }

    //! @brief Return the sender
    [[nodiscard]] std::string getSender() const {
        return mMessage.getSender();
    }

private:
    Message mMessage {};
    Ret mValue {};
    Status mStatus { StatusCode::UNKNOWN_ERROR };
};

//! @brief Specialization for void-returning calls: no payload to parse, so
//! the base status/error accessors are reused as-is.
template<>
class Reply<void> : private Message {
public:
    using Message::Message;
    using Message::status;
    using Message::isError;
    using Message::errorMessage;
    using Message::getSender;
};

}
#endif