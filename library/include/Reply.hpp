#ifndef DBUSXX_DBUS_REPLY_HPP
#define DBUSXX_DBUS_REPLY_HPP

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
class Reply : public Message {
    static_assert(isValidArgs<Ret>(), "Unsupported value type");
public:
    //! @brief Construct an empty reply.
    Reply() = default;

    /**
     * @brief Construct a reply from an implementation, parsing the payload.
     *
     * @param aImpl shared implementation to wrap
     */
    explicit Reply(std::shared_ptr<Private::MessagePrivate> aImpl)
        : Message(std::move(aImpl)) {
            mStatus = read(mValue);
    }

    /**
     * @brief Construct a reply by moving in an implementation and parsing it.
     *
     * @param aImpl implementation to move from
     */
    explicit Reply(Private::MessagePrivate&& aImpl)
        : Message(std::make_shared<Private::MessagePrivate>(std::move(aImpl))) {
            mStatus = read(mValue);
    }

    Reply(const Reply& aOther) = default;

    Reply(Reply&& aOther) noexcept = default;

    Reply& operator=(const Reply&) = default;

    Reply& operator=(Reply&&) = default;

    //! @brief Return the parsed return value (only valid when `isError()` is false).
    [[nodiscard]] inline Ret value() const {
        return mValue;
    }

    //! @brief Return the overall status of the call/reply.
    [[nodiscard]] inline Status status() const {
        return Message::isError() ? Message::status() : mStatus;
    }

    //! @brief Return true if the reply indicates an error.
    [[nodiscard]] inline bool isError() const {
        return mStatus.isError() || Message::isError();
    }

    //! @brief Return the error description when `isError()` is true.
    [[nodiscard]] inline std::string errorMessage() const {
        return Message::isError() ? Message::errorMessage() : mStatus.message();
    }

private:
    Ret mValue {};
    Status mStatus { StatusCode::SUCCESS };
};

//! @brief Specialization for void-returning calls (no payload value).
template<>
class Reply<void> : public Message {
public:
    using Message::Message;
};

}
#endif