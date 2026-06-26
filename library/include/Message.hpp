#ifndef SSDBUS_DBUS_MESSAGE_HPP
#define SSDBUS_DBUS_MESSAGE_HPP

#include <memory>
#include <systemd/sd-bus.h>
#include <type_traits>
#include <tuple>
#include <utility>

#include "Status.hpp"
#include "DbusArgs.hpp"

#include "message/MessagePrivate.hpp"

namespace SSDbus {
class Message {
public:
    Message() = default;

    explicit Message(std::shared_ptr<Private::MessagePrivate> aImpl)
        : mPrivate(std::move(aImpl)) {}

    explicit Message(Private::MessagePrivate&& aImpl)
        : Message(std::make_shared<Private::MessagePrivate>(std::move(aImpl))) {}

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
    Message& operator<<(T& aVal) {
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

    std::string_view getSender() const {
        return mPrivate->getSender();
    }

    bool isError() const {
        return mPrivate->getStatus().isError();
    }

    std::string errorMessage() const {
        return mPrivate->getStatus().message();
    }

private:
    std::shared_ptr<Private::MessagePrivate> mPrivate;
};
}

#endif