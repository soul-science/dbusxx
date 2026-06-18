/********************************************************************************
 * @file DbusContext.cpp
 * @brief Base D-Bus context implementation.
 *
 * Implements common D-Bus setup, teardown and event loop helpers used by
 * service components that interact with systemd and the system bus.
 *
 * @date 2026-03-03
 */

#include "DbusContext.hpp"

#include <fcntl.h>
#include <syslog.h>

#include "Utils.hpp"

namespace SSDbus {

DbusContext::DbusContext(
    const std::string& aServiceName, const std::string& aServicePath, const std::string& aServiceInterface)
: mServiceName(aServiceName)
, mServicePath(aServicePath)
, mServiceInterface(aServiceInterface) {}

DbusContext::~DbusContext() {
    clear();
}

void DbusContext::clear() {
    if (mExitFds[0] >= 0) {
        close(mExitFds[0]);
    }

    if (mExitFds[1] >= 0) {
        close(mExitFds[1]);
    }

    for (auto& slot : mSlots) {
        sd_bus_slot_unref(slot);
    }
    mSlots.clear();

    if (mDbus) {
        sd_bus_unref(mDbus);
        mDbus = nullptr;
    }
        
    if (mEvent) {
        sd_event_unref(mEvent);
        mEvent = nullptr;
    }
}

void DbusContext::run() {
    if (!setupDBus()) {
        return;
    }

    if (!init()) {
        return;
    }

    eventloop();
}

void DbusContext::stop() {

    quitEventLoop();
}

bool DbusContext::setupDBus() {
   //! Create an event loop object
    auto ret = sd_event_new(&mEvent);
    if (ret < 0) {
       syslog(LOG_ERR,
            "Failed to create event: %s\n", strerror(-ret));
        clear();
        return false;
    }

    //! Create event loop object a and open a connection to the system bus
    ret = sd_bus_open_system(&mDbus);
    if (ret < 0) {
        syslog(LOG_ERR,
            "Failed to connect to system bus: %s\n", strerror(-ret));
        clear();
        return false;
    }

    //! Hang dbus in the event loop
    ret = sd_bus_attach_event(mDbus, mEvent, 0);
    if (ret < 0) {
        syslog(LOG_ERR,
            "sd_bus_attach_event: %s", strerror(-ret));
        clear();
        return false;
    }

    //! Register dbus service name
    if (!mServiceName.empty()) {
        ret = sd_bus_request_name(mDbus, mServiceName.c_str(), 0);
        if (ret < 0) {
            syslog(LOG_ERR,
                "Failed to register object to bus: %s\n", strerror(-ret));
            clear();
            return false;
        }
    }

    return true;
}

bool DbusContext::eventloop() {
    if (!mDbus || !mEvent) {
        return false;
    }

    if (pipe(mExitFds) < 0) {
        syslog(LOG_ERR, "pipe failed: %s", strerror(errno));
        return false;
    }

    //! Set the reading end to non blocking
    fcntl(mExitFds[0], F_SETFL, O_NONBLOCK);
    //! Set the seting end to non blocking
    fcntl(mExitFds[1], F_SETFL, O_NONBLOCK);

    auto ret = sd_event_add_io(mEvent, nullptr, mExitFds[0], EPOLLIN,
        [] (sd_event_source*, int aFd, uint32_t aRevent, void *aUserdata) -> int {
            char buf[1];
            if (SSDbus::__safeRead(aFd, buf, 1) <= 0) {
                return -errno;
            }

            auto context = static_cast<DbusContext*>(aUserdata);

            context->deInit();

            sd_event_exit(context->getEvent(), 0);
            return 1;
        }, this
    );
    if (ret < 0) {
        syslog(LOG_ERR,
            "sd_event_add_io failed: %s\n", strerror(-ret));
        return false;
    }

    ret = sd_event_loop(mEvent);
    if (ret < 0) {
        syslog(LOG_ERR,
            "Failed to create event loop: %s\n", strerror(-ret));
        sd_bus_unref(mDbus);
        sd_event_unref(mEvent);
        return false;
    }

    return true;
}

void DbusContext::quitEventLoop() {
    if (SSDbus::__safeWrite(mExitFds[1], "\0", 1) < 0) {
        syslog(LOG_ERR, "quitEventLoop failed");
    }
}

}
