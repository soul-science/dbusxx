#ifndef SSDBUS_DBUS_MESSAGE_HPP
#define SSDBUS_DBUS_MESSAGE_HPP

#include <systemd/sd-bus.h>
#include <type_traits>
#include <tuple>
#include <utility>

#include "DbusReturnStatus.hpp"
#include "DbusArgs.hpp"

namespace SSDbus {
class DbusMessage {
    using Status = DbusReturnStatus::Status;
public:
    DbusMessage() = default;

    explicit DbusMessage(sd_bus_message* aRawMsg, bool aIsOwned = false)
        : mRawMsg(aRawMsg)
        , mIsOwned(aIsOwned) {}

    ~DbusMessage() {
        if (mRawMsg && mIsOwned) {
            sd_bus_message_unref(mRawMsg);
        }
    }

    DbusMessage(DbusMessage&& aOther) noexcept
        : mRawMsg(aOther.mRawMsg) {
            aOther.mRawMsg = nullptr;
    }

    DbusMessage& operator=(DbusMessage&& aOther) {
        if (this == &aOther) {
            return *this;
        }

        if (mRawMsg) {
            sd_bus_message_unref(mRawMsg);
        }

        mRawMsg = aOther.mRawMsg;
        aOther.mRawMsg = nullptr;
        return *this;
    }

    DbusMessage(const DbusMessage&) = delete;
    DbusMessage& operator=(const DbusMessage&) = delete;

    template<typename T>
    DbusMessage& operator>>(T& aVal) {
        read(aVal);
        return *this;
    }

    template<typename T>
    DbusMessage& operator<<(T& aVal) {
        write(aVal);
        return *this;
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
                mRawMsg, DbusTypeSignature<T>::value, &str);
            aVal = str;
        }
        else {
            ret = sd_bus_message_read_basic(
                mRawMsg, DbusTypeSignature<T>::value, &aVal);
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
        if (!mRawMsg) {
            return DbusReturnStatus(Status::FAIL);
        }

        int ret;
        if constexpr (std::is_same_v<T, std::string>
            || std::is_same_v<T, std::string_view>) {
            ret = sd_bus_message_append_basic(
                mRawMsg, DbusTypeSignature<T>::value, aVal.data());
        }
        else {
            ret = sd_bus_message_append_basic(
                mRawMsg, DbusTypeSignature<T>::value, aVal);
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

private:
    sd_bus_message* mRawMsg { nullptr };
    bool mIsOwned { false };
};
}

#endif