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
        RegisterFunc registerFn;
    };

protected:
    using Self = Derived;

    static std::vector<MethodEntry>& registry() {
        static std::vector<MethodEntry> reg;
        return reg;
    }
};
}
#endif