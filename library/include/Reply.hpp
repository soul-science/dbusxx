#ifndef SSDBUS_DBUS_REPLY_HPP
#define SSDBUS_DBUS_REPLY_HPP

#include "Message.hpp"


namespace SSDbus {
template<typename Ret>
class Reply : public Message {
    static_assert(isValidArgs<Ret>(), "Unsupported value type");
public:
    Reply() = default;

    explicit Reply(std::shared_ptr<Private::MessagePrivate> aImpl)
        : Message(std::move(aImpl)) {
            mStatus = read(mValue);
    }

    explicit Reply(Private::MessagePrivate&& aImpl)
        : Message(std::make_shared<Private::MessagePrivate>(std::move(aImpl))) {
            mStatus = read(mValue);
    }

    Reply(const Reply& aOther) = default;

    Reply(Reply&& aOther) noexcept = default;

    Reply& operator=(const Reply&) = default;

    Reply& operator=(Reply&&) = default;

    [[nodiscard]] inline Ret value() const {
        return mValue;
    }

    [[nodiscard]] inline Status status() const {
        return Message::isError() ? Message::status() : mStatus;
    }

    [[nodiscard]] inline bool isError() const {
        return mStatus.isError() || Message::isError();
    }

    [[nodiscard]] inline std::string errorMessage() const {
        return Message::isError() ? Message::errorMessage() : mStatus.message();
    }

private:
    Ret mValue {};
    Status mStatus { StatusCode::SUCCESS };
};

template<>
class Reply<void> : public Message {
public:
    using Message::Message;
};

}
#endif