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
}

#endif //! SSDBUS_UTILS_HPP