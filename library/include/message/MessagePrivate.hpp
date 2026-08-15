#ifndef SSDBUS_MESSAGE_PRIVATE_HPP
#define SSDBUS_MESSAGE_PRIVATE_HPP

#include <string>
#include <string_view>
#include <type_traits>
#include <iostream>

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawMessageSharePtr.hpp"
#include "adaptor/DbusArgs.hpp"
#include "Status.hpp"


namespace SSDbus {
namespace Private {
class MessagePrivate {
public:
    MessagePrivate() = default;

    explicit MessagePrivate(Adaptor::RawMessageSharePtr aPtr);

    ~MessagePrivate() = default;

    MessagePrivate(MessagePrivate&& aOther) noexcept = default;
    MessagePrivate& operator=(MessagePrivate&& aOther) noexcept = default;

    MessagePrivate(const MessagePrivate&) = default;
    MessagePrivate& operator=(const MessagePrivate&) = default;

    inline Adaptor::RawBusMessagePtr rawMessage() {
        return mRawMsg.get();
    }

    template<typename T>
    Status read(T& aVal) {
        if (!mRawMsg) {
            std::cerr << "[DEBUG] Status created as UNKNOWN_ERROR" << std::endl;
            return Status(StatusCode::UNKNOWN_ERROR);
        }

        Status st;
        using rawType = std::decay_t<T>;

        std::string nextType = Adaptor::RawMessage::nextType(mRawMsg.get());
        if (nextType.front() == SD_BUS_TYPE_VARIANT) {
            st = Adaptor::RawMessage::enterContainer(
                mRawMsg.get(), SD_BUS_TYPE_VARIANT,
                getSignature<rawType>().c_str()
            );

            if (st.isError()) {
                return st;
            }

            st = read(aVal);
            if (st.isError()) {
                return st;
            }

            return Adaptor::RawMessage::exitContainer(mRawMsg.get());
        }

        if constexpr (std::is_same_v<rawType, std::string>) {
            const char* str;
            st = Adaptor::RawMessage::popBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, str);
            aVal = str;
        }
        else if constexpr (isVectorV<rawType>) {
            using ElemType = typename rawType::value_type;
            st = Adaptor::RawMessage::enterContainer(
                mRawMsg.get(), SD_BUS_TYPE_ARRAY, getSignature<ElemType>().c_str());
            if (st.isError()) {
                return st;
            }

            while (!Adaptor::RawMessage::isEnd(mRawMsg.get(), false)) {
                ElemType elem {};
                st = read(elem);
                if (st.isError()) {
                    return st;
                }

                aVal.push_back(std::move(elem));
            }

            st = Adaptor::RawMessage::exitContainer(mRawMsg.get());
        }
        else if constexpr (isArrayV<rawType>) {
            using ElemType = typename rawType::value_type;
            st = Adaptor::RawMessage::enterContainer(
                mRawMsg.get(), SD_BUS_TYPE_ARRAY, getSignature<ElemType>().c_str());
            if (st.isError()) {
                return st;
            }

            size_t size = aVal.size();
            size_t idx = 0;
            while (!Adaptor::RawMessage::isEnd(mRawMsg.get(), false)) {
                if (idx >= size) {
                    return Status(StatusCode::INVALID_ARG);
                }

                ElemType elem {};
                st = read(elem);
                if (st.isError()) {
                    return st;
                }

                aVal[idx++] = std::move(elem);
            }

            st = Adaptor::RawMessage::exitContainer(mRawMsg.get());
            if (idx != size) {
                return Status(StatusCode::INVALID_ARG);
            }
        }
        else if constexpr (isMapV<rawType>) {
            using keyType = typename rawType::key_type;
            using valueType = typename rawType::mapped_type;
            st = Adaptor::RawMessage::enterContainer(
                mRawMsg.get(), SD_BUS_TYPE_ARRAY,
                nextType.substr(1).c_str());
            if (st.isError()) {
                return st;
            }

            while (!Adaptor::RawMessage::isEnd(mRawMsg.get(), false)) {
                st = Adaptor::RawMessage::enterContainer(
                    mRawMsg.get(), SD_BUS_TYPE_DICT_ENTRY, nullptr);
                if (st.isError()) {
                    return st;
                }

                keyType key{};
                st = read(key);
                if (st.isError()) {
                    return st;
                }

                valueType val{};
                st = read(val);
                if (st.isError()) {
                    return st;
                }

                aVal[key] = std::move(val);
                st = Adaptor::RawMessage::exitContainer(mRawMsg.get());
                if (st.isError()) {
                    return st;
                }
            }

            st = Adaptor::RawMessage::exitContainer(mRawMsg.get());
        }
        else {
            st = Adaptor::RawMessage::popBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, aVal);
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
        using rawType = std::decay_t<T>;
        if constexpr (std::is_same_v<rawType, std::string>) {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, aVal.c_str());
        }
        else if constexpr (std::is_same_v<rawType, std::string_view>) {
            //! string_view::data() No guarantee that it ends with \0
            //! But the D-Bus type 's' requires a null-terminated C string
            //! So convert it to string, and use c_str
            std::string tmp(aVal);
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, tmp.c_str());
        }
        else if constexpr (std::is_same_v<rawType, const char*>
            || std::is_same_v<rawType, char*>) {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, aVal);
        }
        else if constexpr (std::is_same_v<rawType, float>) {
            //! Convert float to double (unified use of double type)
            //! Ensure that the number of bytes read is consistent
            double d = static_cast<double>(aVal);
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), BasicSignature<rawType>::value,
                &d
            );
        }
        else if constexpr (isVectorV<rawType> || isArrayV<rawType>) {
            using ElemType = typename rawType::value_type;
            st = Adaptor::RawMessage::openContainer(
                mRawMsg.get(), SD_BUS_TYPE_ARRAY, getSignature<ElemType>().c_str());
            if (st.isError()) {
                return st;
            }

            for (const auto& elem : aVal) {
                st = write(elem);
                if (st.isError()) {
                    return st;
                }
            }

            st = Adaptor::RawMessage::closeContainer(mRawMsg.get());
        }
        else if constexpr (isMapV<rawType>) {
            using keyType = typename rawType::key_type;
            using valueType = typename rawType::mapped_type;
            std::string typeBuilder;
            typeBuilder.append(1, SD_BUS_TYPE_DICT_ENTRY_BEGIN)
                .append(getSignature<keyType>())
                .append(getSignature<valueType>())
                .append(1, SD_BUS_TYPE_DICT_ENTRY_END);

            std::string innerTypeBuilder;
            innerTypeBuilder.append(getSignature<keyType>())
                .append(getSignature<valueType>());

            st = Adaptor::RawMessage::openContainer(
                mRawMsg.get(), SD_BUS_TYPE_ARRAY, typeBuilder.c_str());
            if (st.isError()) {
                return st;
            }

            for (const auto& [key, val] : aVal) {
                st = Adaptor::RawMessage::openContainer(
                    mRawMsg.get(), SD_BUS_TYPE_DICT_ENTRY, innerTypeBuilder.c_str());
                if (st.isError()) {
                    return st;
                }

                st = write(key);
                if (st.isError()) {
                    return st;
                }

                st = write(val);
                if (st.isError()) {
                    return st;
                }

                st = Adaptor::RawMessage::closeContainer(mRawMsg.get());
                if (st.isError()) {
                    return st;
                }
            }

            st = Adaptor::RawMessage::closeContainer(mRawMsg.get());
        }
        else {
            st = Adaptor::RawMessage::appendBasic(
                mRawMsg.get(), BasicSignature<rawType>::value, &aVal);
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

    std::string getSender() const;

    inline void setStatus(Status aStatus) {
        mStatus = aStatus;
    }

    inline Status getStatus() const {
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