#include "private/session/LooperPrivate.hpp"

#include <sys/eventfd.h>
#include "private/method/Reconnect.hpp"


namespace Dbusxx {
namespace Private {
namespace {
static ssize_t __safeRead(int aFd, void* aBuffer, size_t aCount) {
    ssize_t n;
    do {
        n = read(aFd, aBuffer, aCount);
    } while (n == -1 && errno == EINTR);

    //! > 0: number of bytes read, 0: EOF, -1: other error
    return n;
}

ssize_t __safeWrite(int aFd, const void* aBuffer, size_t aCount) {
    const char* ptr = static_cast<const char*>(aBuffer);
    size_t remaining = aCount;

    while (remaining > 0) {
        ssize_t written = write(aFd, ptr, remaining);
        if (written == -1) {
            if (errno == EINTR) {
                //! Interrupted by a signal, retry
                continue;
            }

            //! Other errors occur
            return -1;
        }

        ptr += written;
        remaining -= written;
    }

    //! All data has been written to file
    return aCount;
}
}

LooperPrivate::LooperPrivate(SessionPrivate* aSession)
    : mSession(aSession)
    , mEvent(Adaptor::RawEventSharePtr::make())
    , mWakeFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , mExitFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , mStatus(mEvent.status()) {}

LooperPrivate::~LooperPrivate() {
    if (mSession && mSession->rawBus() && mEvent.get()) {
        Adaptor::RawBus::detachEvent(mSession->rawBus().get());
    }

    if (mWakeSrc) {
        Adaptor::RawEventSrc::enableEventSrc(mWakeSrc, SD_EVENT_OFF);
        Adaptor::RawEventSrc::unrefEventSrc(mWakeSrc);
        mWakeSrc = nullptr;
    }

    if (mAcceptSrc) {
        Adaptor::RawEventSrc::enableEventSrc(mAcceptSrc, SD_EVENT_OFF);
        Adaptor::RawEventSrc::unrefEventSrc(mAcceptSrc);
        mAcceptSrc = nullptr;
    }

    if (mWakeFd >= 0) {
        close(mWakeFd);
        mWakeFd = -1;
    }

    if (mExitSrc) {
        Adaptor::RawEventSrc::enableEventSrc(mExitSrc, SD_EVENT_OFF);
        Adaptor::RawEventSrc::unrefEventSrc(mExitSrc);
        mExitSrc = nullptr;
    }

    if (mExitFd >= 0) {
        close(mExitFd);
        mExitFd = -1;
    }
}

void LooperPrivate::run() {
    if (mStatus.load().isError()) {
        return;
    }

    mStatus = bindDaemonDisconnectedSignal();
    if (mStatus.load().isError()) {
        return;
    }

    mStatus = bindExitEntry();
    if (mStatus.load().isError()) {
        return;
    }

    mStatus = bindWakeEntry();
    if (mStatus.load().isError()) {
        return;
    }

    if (mSession->listenFd() >= 0) {
        //! Peer session
        mSession->setPeerAcceptedCallback(std::move(mReadyCallBack));
        int listenFd = mSession->listenFd();
        mStatus = Adaptor::RawEvent::addIO(mEvent.get(), mAcceptSrc,
            listenFd, EPOLLIN,
            [] (Adaptor::RawBusEventSrcPtr, int aFd, uint32_t, void* aData) -> int {
                auto* self = static_cast<LooperPrivate*>(aData);
                Adaptor::RawEventSrc::enableEventSrc(self->mAcceptSrc, SD_EVENT_OFF);
                Adaptor::RawEventSrc::unrefEventSrc(self->mAcceptSrc);
                self->mAcceptSrc = nullptr;

                self->mStatus = self->mSession->acceptPeerConnection(self->mEvent.get());
                return self->mStatus.load().isSuccess() ? 1 : -1;
            }, this);
        if (mStatus.load().isError()) {
            return;
        }
    } else {
        mStatus = Adaptor::RawBus::attachEvent(
            mSession->rawBus().get(), mEvent.get(), 0);
        if (mStatus.load().isError()) {
            return;
        }

        if (mReadyCallBack) {
            post([cb = std::move(mReadyCallBack)]() -> Status {
                return cb();
            });
        }
    }

    mThreadId = std::this_thread::get_id();
    Status st = Adaptor::RawEvent::loop(mEvent.get());
    if (mStatus.load().isSuccess()) {
        mStatus = st;
    }
}

void LooperPrivate::stop() {
    __safeWrite(mExitFd, &EVENT_FD_SIGNAL, sizeof(EVENT_FD_SIGNAL));
}

void LooperPrivate::post(std::function<void()> aTask) {
    {
        std::lock_guard lock(mTaskMutex);
        mTasks.push_back(std::move(aTask));
    }

    //! Write eventfd to awake looper
    __safeWrite(mWakeFd, &EVENT_FD_SIGNAL, sizeof(EVENT_FD_SIGNAL));
}

Status LooperPrivate::bindExitEntry() {
    if (mExitFd < 0) {
        mExitFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (mExitFd < 0) {
            return Adaptor::RawErrorConvert::fromErrno(errno);
        }
    }

    return Adaptor::RawEvent::addIO(mEvent.get(), mExitSrc, mExitFd, EPOLLIN,
        [] (Adaptor::RawBusEventSrcPtr, int aFd, uint32_t aRevent, void *aData) -> int {
            uint64_t cnt;
            if (__safeRead(aFd, &cnt, sizeof(cnt)) <= 0) {
                return -errno;
            }

            auto context = static_cast<LooperPrivate*>(aData);
            context->mStatus = Adaptor::RawEvent::exit(context->mEvent.get(), 0);
            return 1;
        }, this
    );
}

Status LooperPrivate::bindWakeEntry() {
    if (mWakeFd < 0) {
        mWakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (mWakeFd < 0) {
            return Adaptor::RawErrorConvert::fromErrno(errno);
        }
    }

    return Adaptor::RawEvent::addIO(mEvent.get(), mWakeSrc, mWakeFd, EPOLLIN,
        [] (Adaptor::RawBusEventSrcPtr, int aFd, uint32_t aRevent, void* aData) -> int {
            auto* self = static_cast<LooperPrivate*>(aData);

            uint64_t cnt;
            if (__safeRead(aFd, &cnt, sizeof(cnt)) <= 0) {
                return -errno;
            }
            
            std::deque<std::function<void()>> tasks;
            {
                std::lock_guard lock(self->mTaskMutex);
                tasks.swap(self->mTasks);
            }

            for (auto& task : tasks) {
                task();
            }

            return 1;
        }, this
    );
}

Status LooperPrivate::bindDaemonDisconnectedSignal() {
    if (mSession->type() == SessionType::PEER) {
        return Status(StatusCode::SUCCESS);
    }

    mConnectedSlot = Adaptor::RawSlotSharePtr();
    Adaptor::RawBusSlotPtr slot = nullptr;
    auto st = Adaptor::RawBus::listenSignal(
        mSession->rawBus().get(), slot,
        "org.freedesktop.DBus.Local", "",
        "org.freedesktop.DBus.Local", "Disconnected",
        [] (Adaptor::RawBusMessagePtr, void* aData, Adaptor::RawBusErrorPtr) -> int {
            auto* self = static_cast<LooperPrivate*>(aData);
            self->post([self]() { self->doReconnect(); });
            return 0;
        }, this);
    if (st.isSuccess()) {
        mConnectedSlot = Adaptor::RawSlotSharePtr(slot);
    }
    return st;
}

void LooperPrivate::doReconnect() {
    mConnectedSlot = Adaptor::RawSlotSharePtr();
    if (mSession && mSession->rawBus() && mEvent.get()) {
        Adaptor::RawBus::detachEvent(mSession->rawBus().get());
    }

    auto st = Method::reconnectSession(mSession);
    if (st.isError()) {
        return;
    }

    st = Adaptor::RawBus::attachEvent(
        mSession->rawBus().get(), mEvent.get(), 0);
    if (st.isError()) {
        return;
    }

    bindDaemonDisconnectedSignal();
}
}
}