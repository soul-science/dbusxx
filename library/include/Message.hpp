#ifndef SSDBUS_DBUS_MESSAGE_HPP
#define SSDBUS_DBUS_MESSAGE_HPP

#include <memory>
#include <systemd/sd-bus.h>
#include <type_traits>
#include <tuple>
#include <utility>

#include "DbusReturnStatus.hpp"
#include "DbusArgs.hpp"

#include "message/MessagePrivate.hpp"

namespace SSDbus {
class Message {
    using Status = DbusReturnStatus::Status;
public:
    Message() = default;

    explicit Message(std::shared_ptr<Private::MessagePrivate> aImpl)
        : mPrivate(std::move(aImpl)) {}

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
    DbusReturnStatus read(T& aVal) {
        return mPrivate->read(aVal);
    }

    template<typename First, typename... Rests>
    DbusReturnStatus read(First& aFirst, Rests&... aRests) {
        return mPrivate->read(aFirst, aRests...);
    }

    template<typename... Args>
    DbusReturnStatus read(std::tuple<Args...>& aVals) {
        return mPrivate->read(aVals);
    }

    template<typename T>
    DbusReturnStatus write(const T& aVal) {
        return mPrivate->write(aVal);
    }

    template<typename First, typename... Rests>
    DbusReturnStatus write(const First& aFirst, const Rests&... aRests) {
        return mPrivate->write(aFirst, aRests...);
    }

    std::string_view getSender() const {
        return mPrivate->getSender();
    }

private:
    std::shared_ptr<Private::MessagePrivate> mPrivate;
};
}

#endif