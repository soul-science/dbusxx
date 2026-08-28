#ifndef DBUSXX_UNIX_FD_HPP
#define DBUSXX_UNIX_FD_HPP

#include <unistd.h>


namespace Dbusxx {

class UnixFd {
    static constexpr int INVALID_UNIX_FD { -1 };
public:
    UnixFd() = default;

    explicit UnixFd(int aFd)
        : mFd(aFd) {}

    //! Copy = dup a fresh independent handle to the *same* kernel object
    //! (shares the underlying file description; no data is copied).
    //! Rationale: the library's async machinery (Client/PendingReply uses
    //! make_tuple + shared_future) requires copyable value types, so fd must
    //! be copyable to travel through those paths. dup() failure (EMFILE)
    //! yields -1; each copy owns and closes its own fd number.
    UnixFd(const UnixFd& aOther)
        : mFd(dupOf(aOther.mFd)) {}

    UnixFd& operator=(const UnixFd& aOther) {
        if (this != &aOther) {
            reset(dupOf(aOther.mFd));
        }
        return *this;
    }

    //! Two UnixFd are equal iff they refer to the same fd number.
    inline bool operator==(const UnixFd& aOther) const {
        return mFd == aOther.mFd;
    }

    UnixFd(UnixFd&& aOhter) noexcept
        :mFd(aOhter.release()) {}

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

    inline int release() {
        int fd = mFd;
        mFd = INVALID_UNIX_FD;
        return fd;
    }

    inline void reset(int aFd = INVALID_UNIX_FD) {
        if (mFd >= 0) {
            ::close(mFd);
        }

        mFd = aFd;
    }

private:
    static int dupOf(int aFd) {
        return aFd >= 0 ? ::dup(aFd) : INVALID_UNIX_FD;
    }

    int mFd { INVALID_UNIX_FD };
};


}

#endif