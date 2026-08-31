//! Unit tests for the UnixFd RAII wrapper (library/include/UnixFd.hpp).
//!
//! Covers the handle semantics: copy = dup() (an independent fd number for
//! the *same* kernel object), move = ownership transfer, release/reset, and
//! equality (compares fd numbers). These need no D-Bus daemon — they only
//! exercise fd bookkeeping against pipes.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include "UnixFd.hpp"

using namespace Dbusxx;

namespace {

//! Create a pipe; the read end is returned for wrapping, the write end raw
//! so the test can feed data through it. The read end is set non-blocking.
void makePipe(int& aReadFd, int& aWriteFd) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
    aReadFd = fds[0];
    aWriteFd = fds[1];
}

} //! namespace

TEST(UnixFdTest, DefaultIsInvalid) {
    UnixFd fd;
    EXPECT_EQ(fd.get(), -1);
    EXPECT_EQ(fd.release(), -1);
}

TEST(UnixFdTest, ExplicitCtorTakesOwnership) {
    int r, w;
    makePipe(r, w);

    UnixFd fd(r);
    EXPECT_EQ(fd.get(), r);

    fd.reset();                       //! closes r
    EXPECT_EQ(fd.get(), -1);
    EXPECT_EQ(::close(w), 0);
}

TEST(UnixFdTest, CopyDuplicatesHandle) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    UnixFd b = a;                     //! copy → dup()
    EXPECT_GE(b.get(), 0);
    EXPECT_NE(b.get(), a.get());      //! independent fd numbers

    //! Functional: both are the same pipe read end.
    ASSERT_EQ(::write(w, "hello", 5), 5);
    char buf1[8] = {0};
    EXPECT_EQ(::read(a.get(), buf1, 5), 5);
    EXPECT_STREQ(buf1, "hello");

    ASSERT_EQ(::write(w, "abc", 3), 3);
    char buf2[8] = {0};
    EXPECT_EQ(::read(b.get(), buf2, 3), 3);
    EXPECT_STREQ(buf2, "abc");

    ::close(w);
}

TEST(UnixFdTest, CopyAssignDuplicates) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    UnixFd b;
    b = a;                            //! copy-assign → dup()
    EXPECT_GE(b.get(), 0);
    EXPECT_NE(b.get(), a.get());

    ::close(w);
}

TEST(UnixFdTest, MoveTransfersOwnership) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    const int original = a.get();
    UnixFd b(std::move(a));
    EXPECT_EQ(b.get(), original);
    EXPECT_EQ(a.get(), -1);           //! source invalidated

    ::close(w);
}

TEST(UnixFdTest, MoveAssignTransfersOwnership) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    const int original = a.get();
    UnixFd b;
    b = std::move(a);
    EXPECT_EQ(b.get(), original);
    EXPECT_EQ(a.get(), -1);

    ::close(w);
}

TEST(UnixFdTest, ReleaseRelinquishesOwnership) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    const int released = a.release();
    EXPECT_EQ(released, r);
    EXPECT_EQ(a.get(), -1);           //! no longer owns it

    //! The caller now owns `released`; a must not close it on destruction.
    UnixFd b;
    b.reset(released);                //! b takes ownership instead
    EXPECT_EQ(b.get(), released);

    ::close(w);
}

TEST(UnixFdTest, EqualityComparesFdNumbers) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    UnixFd same(r);                   //! two wrappers, same number
    UnixFd dup = a;                   //! dup() → different number, same object

    EXPECT_TRUE(a == same);
    EXPECT_FALSE(a == dup);

    ::close(w);
}

TEST(UnixFdTest, ResetClosesPrevious) {
    int r, w;
    makePipe(r, w);

    UnixFd a(r);
    const int old = a.get();
    EXPECT_EQ(::fcntl(old, F_GETFD), 0);   //! open before reset
    a.reset(-1);
    EXPECT_EQ(a.get(), -1);
    EXPECT_EQ(::fcntl(old, F_GETFD), -1);  //! closed → EBADF

    ::close(w);
}
