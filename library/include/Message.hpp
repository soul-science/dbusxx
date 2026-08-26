#ifndef DBUSXX_DBUS_MESSAGE_HPP
#define DBUSXX_DBUS_MESSAGE_HPP

#include <memory>
#include <tuple>

#include "private/message/MessagePrivate.hpp"
#include "Status.hpp"


namespace Dbusxx {
/**
 * @brief A D-Bus message.
 *
 * `Message` is a type-safe, stream-like wrapper around a raw D-Bus
 * message. Use `operator<<`/`write()` to append arguments and
 * `operator>>`/`read()` to extract them. It is also the base class
 * of #Reply.
 */
class Message {
public:
    //! @brief Construct an empty (invalid) message.
    Message() = default;

    /**
     * @brief Construct a message from an existing implementation.
     *
     * @param aImpl shared implementation to wrap
     */
    explicit Message(std::shared_ptr<Private::MessagePrivate> aImpl);

    /**
     * @brief Construct a message by moving in an implementation.
     *
     * @param aImpl implementation to move from
     */
    explicit Message(Private::MessagePrivate&& aImpl);

    ~Message() = default;

    Message(Message&& aOther) noexcept = default;
    Message& operator=(Message&& aOther) noexcept = default;

    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    /**
     * @brief Extract a value of type `T` from the payload (stream-style).
     *
     * @tparam T   value type
     * @param aVal out-parameter receiving the value
     * @return *this for chaining
     */
    template<typename T>
    Message& operator>>(T& aVal) {
        read(aVal);
        return *this;
    }

    /**
     * @brief Append `aVal` to the payload (stream-style).
     *
     * @tparam T   value type
     * @param aVal value to append
     * @return *this for chaining
     */
    template<typename T>
    Message& operator<<(const T& aVal) {
        write(aVal);
        return *this;
    }

    /**
     * @brief Read a single value of type `T` from the payload.
     *
     * @tparam T   value type
     * @param aVal out-parameter receiving the value
     * @return Status of the read
     */
    template<typename T>
    [[nodiscard]] Status read(T& aVal) {
        return mPrivate->read(aVal);
    }

    /**
     * @brief Read multiple values of possibly different types from the payload.
     *
     * @param aFirst first out-parameter
     * @param aRests remaining out-parameters
     * @return Status of the read
     */
    template<typename First, typename... Rests>
    [[nodiscard]] Status read(First& aFirst, Rests&... aRests) {
        return mPrivate->read(aFirst, aRests...);
    }

    /**
     * @brief Read a `std::tuple` of values from the payload.
     *
     * @tparam Args tuple element types
     * @param aVals out-parameter receiving the values
     * @return Status of the read
     */
    template<typename... Args>
    [[nodiscard]] Status read(std::tuple<Args...>& aVals) {
        return mPrivate->read(aVals);
    }

    /**
     * @brief Append a single value of type `T` to the payload.
     *
     * @tparam T   value type
     * @param aVal value to append
     * @return Status of the write
     */
    template<typename T>
    [[nodiscard]] Status write(const T& aVal) {
        return mPrivate->write(aVal);
    }

    /**
     * @brief Append multiple values of possibly different types to the payload.
     *
     * @param aFirst first value to append
     * @param aRests remaining values to append
     * @return Status of the write
     */
    template<typename First, typename... Rests>
    [[nodiscard]] Status write(const First& aFirst, const Rests&... aRests) {
        return mPrivate->write(aFirst, aRests...);
    }

    //! @brief Return the unique name of the message sender (empty if unknown).
    [[nodiscard]] inline std::string getSender() const {
        return mPrivate ? mPrivate->getSender() : std::string();
    }

    //! @brief Return true if the message represents an error reply.
    [[nodiscard]] inline bool isError() const {
        return mPrivate && mPrivate->getStatus().isError();
    }

    //! @brief Return the transport/parse status of the message.
    [[nodiscard]] inline Status status() const {
        return mPrivate ? mPrivate->getStatus()
                        : Status(StatusCode::UNKNOWN_ERROR);
    }

    //! @brief Return the error description if the message is an error.
    [[nodiscard]] inline std::string errorMessage() const {
        return mPrivate ? mPrivate->getStatus().message() : std::string();
    }

private:
    std::shared_ptr<Private::MessagePrivate> mPrivate;
};
}

#endif