#ifndef DBUSXX_DBUS_MESSAGE_HPP
#define DBUSXX_DBUS_MESSAGE_HPP

#include <memory>
#include <tuple>

#include "private/message/MessagePrivate.hpp"
#include "Status.hpp"


namespace Dbusxx {
class Message {
public:
    Message() = default;

    explicit Message(std::shared_ptr<Private::MessagePrivate> aImpl);

    explicit Message(Private::MessagePrivate&& aImpl);

    ~Message() = default;

    Message(Message&& aOther) noexcept = default;
    Message& operator=(Message&& aOther) noexcept = default;

    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    template<typename T>
    Message& operator>>(T& aVal) {
        read(aVal);
        return *this;
    }

    template<typename T>
    Message& operator<<(const T& aVal) {
        write(aVal);
        return *this;
    }

    template<typename T>
    Status read(T& aVal) {
        return mPrivate->read(aVal);
    }

    template<typename First, typename... Rests>
    Status read(First& aFirst, Rests&... aRests) {
        return mPrivate->read(aFirst, aRests...);
    }

    template<typename... Args>
    Status read(std::tuple<Args...>& aVals) {
        return mPrivate->read(aVals);
    }

    template<typename T>
    Status write(const T& aVal) {
        return mPrivate->write(aVal);
    }

    template<typename First, typename... Rests>
    Status write(const First& aFirst, const Rests&... aRests) {
        return mPrivate->write(aFirst, aRests...);
    }

    inline std::string_view getSender() const {
        return mPrivate->getSender();
    }

    inline bool isError() const {
        return mPrivate->getStatus().isError();
    }

    inline Status status() const {
        return mPrivate->getStatus();
    }

    inline std::string errorMessage() const {
        return mPrivate->getStatus().message();
    }

private:
    std::shared_ptr<Private::MessagePrivate> mPrivate;
};
}

#endif