/********************************************************************************
 * @file DbusManager.cpp
 * @brief D-Bus helper utilities implementation.
 *
 * Contains helper wrappers to register/unregister D-Bus interfaces and
 * emit or match signals.
 *
 * @date 2026-03-03
 */

#include "DbusManager.hpp"

#include <syslog.h>

namespace SSDbus {

void DbusManager::unregisterInterface(sd_bus_slot* aSlot) {
    if (!aSlot) {
        return;
    }

    sd_bus_slot_unref(aSlot);
}

bool DbusManager::emitSignal(const char* aSignal,
                             sd_bus* aBus, const char* aServicePath, const char* aServiceInterface) {
    auto ret = sd_bus_emit_signal(aBus, aServicePath, aServiceInterface, aSignal, nullptr);
    if (ret < 0) {
        syslog(LOG_ERR,
            "Fail to emit %s signal: %s", aSignal, strerror(-ret));
        return false;
    }

    syslog(LOG_DEBUG,
        "Success to emit %s signal", aSignal);
    return true;
}

}
