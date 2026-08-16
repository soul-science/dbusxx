#ifndef SSDBUS_LOOPER_HPP
#define SSDBUS_LOOPER_HPP

#include <functional>

#include "private/session/LooperPrivate.hpp"


namespace SSDbus {
class Session;
class Looper {
public:
    Looper() = default;

    explicit Looper(Session& aSession);

    void run();

    void stop();

    void post(std::function<void()> aTask);

    template<typename Callback>
    void onReady(Callback&& aCallback) {
        mPrivate->onReady(std::forward<Callback>(aCallback));
    }

    inline bool isOwnerThread() const {
        return mPrivate->isOwnerThread();
    }

    inline Status status() const {
        return mPrivate->status();
    }

    inline Session* session() const {
        return mSession;
    }

private:
    Session* mSession {nullptr};
    std::shared_ptr<Private::LooperPrivate> mPrivate { nullptr };
};

}

#endif