/********************************************************************************
 * @file DbusUtils.hpp
 * @brief utils.
 *
 * Declares some tool utils
 *
 * @date 2026-03-22
 */

#ifndef SSDBUS_UTILS_HPP
#define SSDBUS_UTILS_HPP

#include <unistd.h>
#include <string>

namespace SSDbus {

struct ServiceInfo {
    std::string name;
    std::string path;
    std::string interface;
};

//! Safe read function
ssize_t __safeRead(int aFd, void* aBuffer, size_t aCount);

//! Safe write function
ssize_t __safeWrite(int aFd, const void* aBuffer, size_t aCount);
}

#endif //! SSDBUS_UTILS_HPP