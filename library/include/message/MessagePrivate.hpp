#ifndef SSDBUS_MESSAGE_PRIVATE_HPP
#define SSDBUS_MESSAGE_PRIVATE_HPP

#include <string>
#include <iostream>

#include "DbusReturnStatus.hpp"

#include "adaptor/RawAdaptor.hpp"
#include "adaptor/RawMessageSharePtr.hpp"

namespace SSDbus {
namespace Private {

class MessagePrivate {
    using Status = SSDbus::DbusReturnStatus::Status;
public:
    MessagePrivate() = default;

    explicit MessagePrivate(Adaptor::RawMessageSharePtr aPtr)
        : mRawMsg(aPtr) {}

    ~MessagePrivate() = default;

    MessagePrivate(MessagePrivate&& aOther) noexcept = default;
    MessagePrivate& operator=(MessagePrivate&& aOther) noexcept = default;

    MessagePrivate(const MessagePrivate&) = default;
    MessagePrivate& operator=(const MessagePrivate&) = default;

    Adaptor::RawBusMessagePtr rawMessage() {
        return mRawMsg.get();
    }

    template<typename T>
    DbusReturnStatus read(T& aVal) {
        if (!mRawMsg) {
            return DbusReturnStatus(Status::FAIL);
        }

        int ret;
        if constexpr (std::is_same_v<T, std::string>) {
            const char* str;
            ret = sd_bus_message_read_basic(
                mRawMsg.get(), DbusTypeSignature<T>::value, &str);
            aVal = str;
        }
        else {
            ret = sd_bus_message_read_basic(
                mRawMsg.get(), DbusTypeSignature<T>::value, &aVal);
        }

        return DbusReturnStatus(ret >= 0 ? Status::SUCCESS : Status::FAIL);
    }

    template<typename First, typename... Rests>
    DbusReturnStatus read(First& aFirst, Rests&... aRests) {
        auto res = read(aFirst);
        if (!res) {
            return res;
        }

        return read(aRests...);
    }

    template<typename... Args>
    DbusReturnStatus read(std::tuple<Args...>& aVals) {
        DbusReturnStatus status;
        [&]<size_t... Idx>(std::index_sequence<Idx...> ) {
            status = (read(std::get<Idx>(aVals))
            , ...);
        }(std::make_index_sequence<sizeof...(Args)>{});
        return status;
    }

    template<typename T>
    DbusReturnStatus write(const T& aVal) {
        if (!mRawMsg.get()) {
            return DbusReturnStatus(Status::FAIL);
        }

        int ret;
        if constexpr (std::is_same_v<T, std::string>
            || std::is_same_v<T, std::string_view>) {
            ret = sd_bus_message_append_basic(
                mRawMsg.get(), DbusTypeSignature<T>::value, aVal.data());
        }
        else if constexpr (std::is_same_v<T, const char*>) {
            ret = sd_bus_message_append_basic(
                mRawMsg.get(), DbusTypeSignature<T>::value, aVal);
        }
        else {
            ret = sd_bus_message_append_basic(
                mRawMsg.get(), DbusTypeSignature<T>::value, &aVal);
        }

        return DbusReturnStatus(ret >= 0 ? Status::SUCCESS : Status::FAIL);
    }

    template<typename First, typename... Rests>
    DbusReturnStatus write(const First& aFirst, const Rests&... aRests) {
        auto res = write(aFirst);
        if (!res) {
            return res;
        }

        return write(aRests...);
    }

    std::string_view getSender() const {
        return Adaptor::RawMessage::getSender(mRawMsg.get());
    }


private:
    Adaptor::RawMessageSharePtr mRawMsg { nullptr };
    Adaptor::RawMessage::Type mType { Adaptor::RawMessage::Type::Invalid };
};

}
}

#endif