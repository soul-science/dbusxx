/****************************************************************************
 * @file DbusContext.hpp
 * @brief Base class for D-Bus contexts used by service components.
 *
 * Declares `DbusContext` which provides common event loop and D-Bus setup
 * utilities for components that interact with the system bus.
 *
 * @date 2026-03-22
 */

#ifndef SSDBUS_CONTEXT_HPP
#define SSDBUS_CONTEXT_HPP

#include <systemd/sd-bus.h>
#include <string>
#include <vector>

namespace SSDbus {

class DbusContext {
public:
    DbusContext(
        const std::string& aServiceName, const std::string& aServicePath, const std::string& aServiceInterface);

    virtual ~DbusContext();

    void run();

    void stop();

    inline std::string getServiceName() {
        return mServiceName;
    }

    inline std::string getServicePath() {
        return mServicePath;
    }

    inline std::string getServiceInterface() {
        return mServiceInterface;
    }

    inline sd_bus* getDbus() {
        return mDbus;
    }

    inline sd_event* getEvent() {
        return mEvent;
    }

    inline std::vector<sd_bus_slot*>& getSlots() {
        return mSlots;
    }

protected:
    virtual bool init() = 0;

    virtual void deInit() = 0;

private:
    bool setupDBus();

    bool eventloop();

    void quitEventLoop();

    void clear();

    std::string mServiceName;
    
    std::string mServicePath;

    std::string mServiceInterface;

    sd_bus* mDbus { nullptr };

    sd_event* mEvent { nullptr };

    std::vector<sd_bus_slot*> mSlots;

    int mExitFds[2];
};

} // namespace SSDbus

#endif //! SSDBUS_CONTEXT_HPP