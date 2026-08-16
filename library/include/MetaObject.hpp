#ifndef SSDBUS_META_OBJECT_HPP
#define SSDBUS_META_OBJECT_HPP

#include <string_view>
#include <vector>


namespace SSDbus {
//! CRTP ensure every class has an independent reg
template<typename Derived>
class MetaObject {
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

#define SSDBUS_PATH(p)                                                                      \
    static inline int _ssdbus_path_##__LINE__ = [] {                                        \
        Self::sPath = p;                                                                    \
        return 0;                                                                           \
    }();

#define SSDBUS_IFACE(i)                                                                     \
    static inline int _ssdbus_iface_##__LINE__ = [] {                                       \
        Self::sIface = i;                                                                   \
        return 0;                                                                           \
    }();

#define SSDBUS_METHOD(method)                                                               \
    static inline int _ssdbus_reg_##method = [] {                                           \
        Self::registry().push_back({                                                        \
            #method,                                                                        \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addMethod(#method, self, &Self::method);                           \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_SIGNAL(signal, ...)                                                          \
    static inline int _ssdbus_reg_##signal = [] {                                           \
        Self::registry().push_back({                                                        \
            #signal,                                                                        \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addSignal<__VA_ARGS__>(#signal);                                   \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_PROPERTY_RO(name, Type, initValue)                                           \
    static inline int _ssdbus_reg_prop_##name = [] {                                        \
        Self::registry().push_back({                                                        \
            #name,                                                                          \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<Type>(#name, initValue, false);                        \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_PROPERTY_RW(name, Type, initValue)                                           \
    static inline int _ssdbus_reg_prop_##name = [] {                                        \
        Self::registry().push_back({                                                        \
            #name,                                                                          \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<Type>(#name, initValue, true);                         \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

}
#endif