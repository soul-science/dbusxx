#include "Utils.hpp"

#include <cerrno>

namespace SSDbus {

ssize_t __safeRead(int aFd, void* aBuffer, size_t aCount) {
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
