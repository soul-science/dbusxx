#ifndef SSDBUS_MESSAGE_PRIVATE_HPP
#define SSDBUS_MESSAGE_PRIVATE_HPP

#include <string>
#include <iostream>

#include "Status.hpp"

#include "adaptor/RawAdaptor.hpp"
#include "adaptor/RawMessageSharePtr.hpp"

namespace SSDbus {
namespace Private {

class MessagePrivate {
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
    Status read(T& aVal) {
        if (!mRawMsg) {
            std::cerr << "[DEBUG] Status created as UNKNOWN_ERROR" << std::endl;
            return Status(StatusCode::UNKNOWN_ERROR);
        }

        Status st;
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            const char* str;
            st = Adaptor::RawMessage::popBasic(
                mRawMsg.get(), DbusTypeSignature<std::decay_t<T>>::value, str);
            aVal = str;
        }
        else {
            st = Adaptor::RawMessage::popBasic(
                mRawMsg.get(), DbusTypeSignature<std::decay_t<T>>::value, aVal);
        }

        return st;
    }

    template<typename First, typename... Rests>
    Status read(First& aFirst, Rests&... aRests) {
        auto st = read(aFirst);
        if (st.isError()) {
            return st;
        }

        return read(aRests...);
    }

    template<typename... Args>
    Status read(std::tuple<Args...>& aVals) {
        Status status;
        [&]<size_t... Idx>(std::index_sequence<Idx...> ) {
            ((status = read(std::get<Idx>(aVals))).isSuccess() && ...);
        }(std::make_index_sequence<sizeof...(Args)>{});
        return status;
    }

    template<typename T>
    Status write(const T& aVal) {
        if (!mRawMsg.get()) {
            return Status(StatusCode::UNKNOWN_ERROR);
        }

        Status st;
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>
            || std::is_same_v<std::decay_t<T>, std::string_view>) {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), DbusTypeSignature<std::decay_t<T>>::value, aVal.data());
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, const char*>
            || std::is_same_v<std::decay_t<T>, char*>) {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), DbusTypeSignature<std::decay_t<T>>::value, aVal);
        }
        else {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), DbusTypeSignature<std::decay_t<T>>::value, &aVal);
        }

        return st;
    }

    template<typename First, typename... Rests>
    Status write(const First& aFirst, const Rests&... aRests) {
        auto st = write(aFirst);
        if (st.isError()) {
            return st;
        }

        return write(aRests...);
    }

    std::string_view getSender() const {
        return Adaptor::RawMessage::getSender(mRawMsg.get());
    }

    void setStatus(Status aStatus) {
        mStatus = aStatus;
    }

    Status getStatus() const {
        return mStatus;
    }

protected:
    Adaptor::RawMessageSharePtr mRawMsg { nullptr };
    Adaptor::RawMessage::Type mType { Adaptor::RawMessage::Type::Invalid };
    Status mStatus { StatusCode::SUCCESS };
};

}
}

#endif