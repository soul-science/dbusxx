#include "Looper.hpp"

#include "Session.hpp"


namespace SSDbus {
Looper::Looper(Session& aSession)
    : mSession(&aSession)
    , mPrivate(
        std::make_shared<Private::LooperPrivate>(aSession.mPrivate.get())) {}

void Looper::run() {
    mPrivate->run();
}

void Looper::stop() {
    mPrivate->stop();
}

void Looper::post(std::function<void()> aTask) {
    mPrivate->post(std::move(aTask));
}
}