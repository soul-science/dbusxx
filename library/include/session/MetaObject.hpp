#ifndef SSDBUS_META_OBJECT_HPP
#define SSDBUS_META_OBJECT_HPP

#include <string_view>
#include <vector>

namespace SSDbus {

//! CRTP ensure every class has an independent reg
template<typename Derived>
class MetaObject {
    friend class Session;
    using RegisterFunc = void(*)(void* aBuilder, void* aObj);

    struct MethodEntry {
        std::string_view name;
        std::string_view path;
        std::string_view iface;
        RegisterFunc registerFn;
    };

protected:
    using Self = Derived;

    inline static const char* sPath  = nullptr;
    inline static const char* sIface = nullptr;

    static std::vector<MethodEntry>& registry() {
        static std::vector<MethodEntry> reg;
        return reg;
    }
};
}
#endif