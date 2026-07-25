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

    enum class EntryType : uint8_t {
        Method,
        Signal,
        SignalListen
    };

    struct MethodEntry {
        std::string_view name;
        uint8_t type;
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

#define SSDBUS_METHOD(method)                                                               \
    static inline int _ssdbus_reg_##method = [] {                                           \
        Self::registry().push_back({                                                        \
            #method,                                                                        \
            0,                                                                              \
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
            1,                                                                              \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::SSDbus::Session::RegisterBuilder*>(aBuilder); \
                builder->addSignal<__VA_ARGS__>(#signal);                                   \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#define SSDBUS_LISTEN(method, sender, path, iface, signal)                                  \
    static inline int _ssdbus_##method_listen_signal = [] {                                 \
        Self::registry().push_back({                                                        \
            #method "_listen_" #signal,                                                     \
            2,                                                                              \
            [](void* aSession, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* session = static_cast<::SSDbus::Session*>(aSession);                  \
                session->listenSignal(sender, path, iface, signal, self, &Self::method);    \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

#endif