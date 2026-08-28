#ifndef DBUSXX_UNIX_FD_HPP
#define DBUSXX_UNIX_FD_HPP

#include <fcntl.h>
#include <unistd.h>

#include "Status.hpp"


namespace Dbusxx {

class UnixFd {
    static constexpr int INVALID_UNIX_FD { -1 };
public:
    UnixFd() = default;

    explicit UnixFd(int aFd)
        : mFd(aFd)
        , mStatus(isValidFd(aFd) ? Status(StatusCode::SUCCESS)
            : Status(StatusCode::INVALID_ARG)) {}

    //! Use dup to copy fd(a new fd of the same file)
    UnixFd(const UnixFd& aOther)
        : mFd(dupOf(aOther.mFd)) {}

    UnixFd& operator=(const UnixFd& aOther) {
        if (this != &aOther) {
            reset(dupOf(aOther.mFd));
        }

        return *this;
    }

    //! Two UnixFd are equal if they refer to the same fd number.
    inline bool operator==(const UnixFd& aOther) const {
        return mFd == aOther.mFd;
    }

    UnixFd(UnixFd&& aOther) noexcept
        :mFd(aOther.release()) {}

    UnixFd& operator=(UnixFd&& aOther) noexcept {
        if (this == &aOther) {
            return *this;
        }

        reset(aOther.release());
        return *this;
    }

    ~UnixFd() {
        reset();
    }

    inline int get() const {
        return mFd;
    }

    inline Status status() const {
        return mStatus;
    }

    inline int release() {
        int fd = mFd;
        mFd = INVALID_UNIX_FD;
        mStatus = Status(StatusCode::INVALID_ARG);
        return fd;
    }

    inline void reset(int aFd = INVALID_UNIX_FD) {
        if (mFd >= 0) {
            ::close(mFd);
        }

        mFd = aFd;
        mStatus = isValidFd(aFd) ? Status(StatusCode::SUCCESS)
            : Status(StatusCode::INVALID_ARG);
    }

private:
    static bool isValidFd(int aFd) {
        return ::fcntl(aFd, F_GETFD) >= 0;
    }

    static int dupOf(int aFd) {
        return aFd >= 0 ? ::dup(aFd) : INVALID_UNIX_FD;
    }

    int mFd { INVALID_UNIX_FD };
    Status mStatus { StatusCode::INVALID_ARG };
};


}

#endif