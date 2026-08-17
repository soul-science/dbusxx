#ifndef DBUSXX_LOOPER_HPP
#define DBUSXX_LOOPER_HPP

#include <functional>

#include "private/session/LooperPrivate.hpp"


namespace Dbusxx {
class Session;
/**
 * @brief An event loop driving a D-Bus #Session.
 *
 * `Looper` integrates the session with sd-event: it dispatches incoming
 * messages and runs tasks posted via `post()` on its owning thread.
 */
class Looper {
public:
    //! @brief Construct an empty (unbound) looper.
    Looper() = default;

    /**
     * @brief Construct a looper that drives the given session.
     *
     * @param aSession session to drive
     */
    explicit Looper(Session& aSession);

    //! @brief Run the event loop (blocks the calling thread).
    void run();

    //! @brief Ask the event loop to stop gracefully.
    void stop();

    /**
     * @brief Post a task to be executed on the loop's thread.
     *
     * @param aTask task to run
     */
    void post(std::function<void()> aTask);

    /**
     * @brief Register a callback run once the loop is ready (or a peer is accepted).
     *
     * @param aCallback callback to invoke
     */
    template<typename Callback>
    void onReady(Callback&& aCallback) {
        if (!mPrivate) {
            return;
        }

        mPrivate->onReady(std::forward<Callback>(aCallback));
    }

    //! @brief Return true if the caller is the thread that owns the loop.
    [[nodiscard]] inline bool isOwnerThread() const {
        return mPrivate ? mPrivate->isOwnerThread() : false;
    }

    //! @brief Return the loop's current status.
    [[nodiscard]] inline Status status() const {
        return mPrivate ? mPrivate->status() : Status(StatusCode::INVALID_ARG);
    }

    //! @brief Return the session this loop drives (may be null).
    inline Session* session() const {
        return mSession;
    }

private:
    Session* mSession {nullptr};
    std::shared_ptr<Private::LooperPrivate> mPrivate { nullptr };
};

}

#endif