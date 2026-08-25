#ifndef DBUSXX_LOOPER_PRIVATE_HPP
#define DBUSXX_LOOPER_PRIVATE_HPP

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

#include "private/adaptor/RawEventSharePtr.hpp"
#include "private/adaptor/RawSlotSharePtr.hpp"
#include "private/session/SessionPrivate.hpp"


namespace Dbusxx {
namespace Private {
class LooperPrivate {
    static constexpr uint64_t EVENT_FD_SIGNAL { 1 };
public:
    LooperPrivate() = default;

    explicit LooperPrivate(SessionPrivate* aSession);

    ~LooperPrivate();

    template<typename Callback>
    void onReady(Callback&& aCallback) {
        using Result = std::invoke_result_t<Callback>;
        static_assert(std::is_void_v<Result>
            || std::is_convertible_v<Result, Status>,
            "onReady callback must return void or Status");

        mReadyCallBack = [cb = std::forward<Callback>(aCallback)] () -> Status {
            if constexpr (std::is_void_v<Result>) {
                cb();
                return Status(StatusCode::SUCCESS);
            } else {
                return cb();
            }
        };
    }

    void run();

    void stop();

    void post(std::function<void()> aTask);

    inline bool isOwnerThread() const {
        return std::this_thread::get_id() == mThreadId.load();
    }

    inline Status status() const {
        return mStatus.load();
    }

private:
    Status bindExitEntry();

    Status bindWakeEntry();

    Status bindDaemonDisconnectedSignal();

    void doReconnect();

    SessionPrivate* mSession;

    int mExitFd { -1 };
    Adaptor::RawEventSharePtr mEvent { nullptr, StatusCode::UNKNOWN_ERROR };
    Adaptor::RawBusEventSrcPtr mExitSrc { nullptr };

    int mWakeFd { -1 };
    Adaptor::RawBusEventSrcPtr mWakeSrc { nullptr };
    Adaptor::RawBusEventSrcPtr mAcceptSrc { nullptr };

    std::mutex mTaskMutex;
    std::atomic<std::thread::id> mThreadId;
    std::deque<std::function<void()>> mTasks;

    std::function<Status()> mReadyCallBack;

    Adaptor::RawSlotSharePtr mConnectedSlot;
    std::atomic<Status> mStatus { StatusCode::UNKNOWN_ERROR };

};
}
}

#endif