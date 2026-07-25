#ifndef SSDBUS_LOOPER_HPP
#define SSDBUS_LOOPER_HPP

#include <functional>

#include "session/LooperPrivate.hpp"
#include "Session.hpp"

namespace SSDbus {

class Looper {
public:
    Looper() = default;

    explicit Looper(const Session& aSession)
    : mPrivate(std::make_shared<Private::LooperPrivate>(aSession.mPrivate.get())) {}

    void run() {
        mPrivate->run();
    }

    void stop() {
        mPrivate->stop();
    }

    void post(std::function<void()> aTask) {
        mPrivate->post(std::move(aTask));
    }

    bool isOwnerThread() const {
        return mPrivate->isOwnerThread();
    }

    Status status() const {
        return mPrivate->status();
    }

private:
    std::shared_ptr<Private::LooperPrivate> mPrivate { nullptr };
};

}

#endif