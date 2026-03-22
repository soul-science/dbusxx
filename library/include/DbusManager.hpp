/********************************************************************************
 * @file DbusManager.hpp
 * @brief D-Bus helper declarations.
 *
 * Declares utility helpers to register D-Bus interfaces, match signals and
 * emit notifications.
 *
 * @date 2026-03-22
 */

#ifndef SSDBUS_MANAGER_HPP
#define SSDBUS_MANAGER_HPP

#include <systemd/sd-bus.h>
#include <memory>
#include <cstdio>
#include <vector>

namespace SSDbus {

//! Tool class, integrating Dbus related services
class DbusManager {

static constexpr const char* MATHCH_FMT {
    "type='signal',sender='{%s}',path='{%s}',interface='{%s}',member='{%s}'"
};

public:
    DbusManager() = default;

    ~DbusManager() = default;

    //! Internal embedding class,
    //! used to wrap interface functions that need to be exposed to dbus
    template<typename Cls>
    struct Wrapper {
        using ClassFuncPtr = int (Cls::*)(sd_bus_message*, void*, sd_bus_error*);

        //! Wrapper function
        static int call(sd_bus_message* aMsg, void* aUserdata, sd_bus_error* aError) {
            auto* pair = static_cast<std::pair<Cls*, ClassFuncPtr>*>(aUserdata);
            Cls* obj = pair->first;
            ClassFuncPtr func = pair->second;
            return (obj->*func)(aMsg, obj, aError);
        }
    };

    template<typename Cls, typename Func>
    static sd_bus_slot* registerInterface(const char* aFuncName,
        const char* aInput, const char* aOuput, Cls* aObject, Func aFunc,
        sd_bus* aBus, const char* aServicePath, const char* aServiceInterface) {
        using ClassFuncPtr = typename Wrapper<Cls>::ClassFuncPtr;
        //! Userdata stores classes and funcs
        auto data = std::make_unique<std::pair<Cls*, ClassFuncPtr>>(aObject, static_cast<ClassFuncPtr>(aFunc));
        auto dataPtr = data.get();

        //! Save userdata in the pool
        static std::vector<std::unique_ptr<std::pair<Cls*, ClassFuncPtr>>> dataPool;
        dataPool.push_back(std::move(data));

        //! Create vtable
        auto vtable = std::unique_ptr<sd_bus_vtable[]>( new sd_bus_vtable[3] {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD(aFuncName, aInput, aOuput, &Wrapper<Cls>::call, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        } );
        sd_bus_vtable* vtablePtr = vtable.get();

        //! Save vtable in pool
        static std::vector<std::unique_ptr<sd_bus_vtable[]>> vtablePool;
        vtablePool.push_back(std::move(vtable));

        sd_bus_slot* slot { nullptr };
        auto ret = sd_bus_add_object_vtable(
            aBus, &slot, aServicePath, aServiceInterface, vtablePtr, dataPtr
        );
        
        return slot;
    }

    static void unregisterInterface(sd_bus_slot* aSlot);

    static bool emitSignal(const char* aSignal,
        sd_bus* aBus, const char* aServicePath, const char* aServiceInterface);

    template<typename Cls, typename Func>
    static bool matchSignal(const char* aSignal, Cls* aObject, Func aCallback,
        sd_bus* aBus, const char* aServiceName, const char* aServicePath, const char* aServiceInterface) {
        using ClassFuncPtr = typename Wrapper<Cls>::ClassFuncPtr;
        //! Userdata stores classes and funcs
        auto data = std::make_unique<std::pair<Cls*, ClassFuncPtr>>(
            aObject, static_cast<ClassFuncPtr>(aCallback));
        auto dataPtr = data.get();    

        //! Save userdata in the pool
        static std::vector<std::unique_ptr<std::pair<Cls*, ClassFuncPtr>>> dataPool;
        dataPool.push_back(std::move(data));

        char match[512] {};
        std::sprintf(match, MATHCH_FMT, aServiceName, aServicePath, aServiceInterface, aSignal);

        auto ret = sd_bus_add_match(
            aBus, nullptr, 
            match,
            &Wrapper<Cls>::call,
            dataPtr
        );

        return ret >= 0;
    };

};

}
#endif //! SSDBUS_MANAGER_HPP