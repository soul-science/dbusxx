#ifndef SSDBUS_LOOPER_PRIVATE_HPP
#define SSDBUS_LOOPER_PRIVATE_HPP

#include <deque>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <sys/eventfd.h>
#include <thread>

#include "session/SessionPrivate.hpp"
#include "adaptor/RawEventSharePtr.hpp"

namespace SSDbus {
namespace Private {

class LooperPrivate {
public:
    LooperPrivate() {}

    explicit LooperPrivate(SessionPrivate* aSession)
        : mSession(aSession)
        , mEvent(Adaptor::RawEventSharePtr::make())
        , mStatus(mEvent.status()) {}

    ~LooperPrivate() {
        if (mSession && mSession->rawBus() && mEvent.get()) {
            Adaptor::RawBus::detachEvent(mSession->rawBus().get());
        }

        if (mWakeSrc) {
            sd_event_source_set_enabled(mWakeSrc, SD_EVENT_OFF);
            sd_event_source_unref(mWakeSrc);
            mWakeSrc = nullptr;
        }

        if (mWakeFd >= 0) {
            close(mWakeFd);
            mWakeFd = -1;
        }

        if (mExitSrc) {
            sd_event_source_set_enabled(mExitSrc, SD_EVENT_OFF);
            sd_event_source_unref(mExitSrc);
            mExitSrc = nullptr;
        }

        if (mExitFds[0] >= 0) {
            close(mExitFds[0]);
        }

        if (mExitFds[1] >= 0) {
            close(mExitFds[1]);
        }
    }

    void run() {
        if (mStatus.isError()) {
            return;
        }

        mStatus = bindExitEntry();
        if (mStatus.isError()) {
            return;
        }

        mStatus = bindWakeEntry();
        if (mStatus.isError()) {
            return;
        }

        mStatus = Adaptor::RawBus::attachEvent(mSession->rawBus().get(), mEvent.get(), 0);
        if (mStatus.isError()) {
            return;
        }

        mThreadId = std::this_thread::get_id();
        mStatus = Adaptor::RawEvent::loop(mEvent.get());
    }

    void stop() {
        if (__safeWrite(mExitFds[1], "\0", 1) < 0) {
            mStatus = Adaptor::RawErrorConvert::fromErrno(errno);
        }
    }

    void post(std::function<void()> aTask){
        {
            std::lock_guard lock(mTaskMutex);
            mTasks.push_back(std::move(aTask));
        }

        //! Write eventfd to awake looper
        uint64_t one = 1;
        __safeWrite(mWakeFd, &one, sizeof(one));
    }

    bool isOwnerThread() const {
        return std::this_thread::get_id() == mThreadId;
    }

    Status status() const {
        return mStatus;
    }

private:
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

    Status bindExitEntry() {
        if (pipe(mExitFds) < 0) {
            return Adaptor::RawErrorConvert::fromErrno(errno);
        }

        //! Set the reading end to non blocking
        fcntl(mExitFds[0], F_SETFL, O_NONBLOCK);
        //! Set the seting end to non blocking
        fcntl(mExitFds[1], F_SETFL, O_NONBLOCK);

        return Adaptor::RawEvent::addIO(mEvent.get(), mExitSrc, mExitFds[0], EPOLLIN,
            [] (Adaptor::RawBusEventSrcPtr, int aFd, uint32_t aRevent, void *aData) -> int {
                char buf[1];
                if (__safeRead(aFd, buf, 1) <= 0) {
                    return -errno;
                }

                auto context = static_cast<LooperPrivate*>(aData);
                context->mStatus = Adaptor::RawEvent::exit(context->mEvent.get(), 0);
                return 1;
            }, this
        );
    }

    Status bindWakeEntry() {
        mWakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (mWakeFd < 0) {
            return Adaptor::RawErrorConvert::fromErrno(errno);
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

    SessionPrivate* mSession;
    Adaptor::RawEventSharePtr mEvent { nullptr, StatusCode::UNKNOWN_ERROR };
    Adaptor::RawBusEventSrcPtr mExitSrc { nullptr };
    int mExitFds[2];

    std::mutex mTaskMutex;
    std::deque<std::function<void()>> mTasks;
    int mWakeFd { -1 };
    Adaptor::RawBusEventSrcPtr mWakeSrc { nullptr };

    std::thread::id mThreadId;

    Status mStatus { StatusCode::UNKNOWN_ERROR };

};
}
}

#endif