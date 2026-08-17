#ifndef DBUSXX_META_OBJECT_HPP
#define DBUSXX_META_OBJECT_HPP

#include <string_view>
#include <vector>


namespace Dbusxx {
/**
 * @brief CRTP base that collects reflection metadata.
 *
 * `MetaObject<Derived>` keeps a per-derived-class registry of
 * methods/signals/properties annotated with the `DBUSXX_*` macros below.
 * `Session::registerObject()` consumes this registry to publish the
 * annotated interface in one step.
 */
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

/**
 * @def DBUSXX_PATH
 * @brief Set the D-Bus object path for subsequent annotations in this class.
 * @param aPath object path to set
 */
#define DBUSXX_PATH(aPath)                                                                  \
    static inline int _dbusxx_path_##__LINE__ = [] {                                        \
        Self::sPath = aPath;                                                                \
        return 0;                                                                           \
    }();

/**
 * @def DBUSXX_IFACE
 * @brief Set the D-Bus interface name for subsequent annotations in this class.
 * @param aIface interface name to set
 */
#define DBUSXX_IFACE(aIface)                                                                \
    static inline int _dbusxx_iface_##__LINE__ = [] {                                       \
        Self::sIface = aIface;                                                              \
        return 0;                                                                           \
    }();

/**
 * @def DBUSXX_METHOD
 * @brief Expose the member function `aMethod` as a D-Bus method.
 * @param aMethod member function name to expose
 */
#define DBUSXX_METHOD(aMethod)                                                              \
    static inline int _dbusxx_reg_##aMethod = [] {                                          \
        Self::registry().push_back({                                                        \
            #aMethod,                                                                       \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::Dbusxx::Session::RegisterBuilder*>(aBuilder); \
                builder->addMethod(#aMethod, self, &Self::aMethod);                         \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

/**
 * @def DBUSXX_SIGNAL
 * @brief Expose a D-Bus signal with the given argument types.
 * @param aSignal signal name to expose
 * @param ...     signal argument types (e.g. `int32_t, std::string`)
 */
#define DBUSXX_SIGNAL(aSignal, ...)                                                         \
    static inline int _dbusxx_reg_##aSignal = [] {                                          \
        Self::registry().push_back({                                                        \
            #aSignal,                                                                       \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* self = static_cast<Self*>(aObj);                                      \
                auto* builder = static_cast<::Dbusxx::Session::RegisterBuilder*>(aBuilder); \
                builder->addSignal<__VA_ARGS__>(#aSignal);                                  \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

/**
 * @def DBUSXX_PROPERTY_RO
 * @brief Expose a read-only property (the wrapper owns its own copy of the value).
 * @param aName      property name to expose
 * @param aType      property value type
 * @param aInitValue initial value
 */
#define DBUSXX_PROPERTY_RO(aName, aType, aInitValue)                                        \
    static inline int _dbusxx_reg_prop_##aName = [] {                                        \
        Self::registry().push_back({                                                        \
            #aName,                                                                         \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::Dbusxx::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<aType>(#aName, aInitValue, false);                     \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

/**
 * @def DBUSXX_PROPERTY_RW
 * @brief Expose a read-write property (the wrapper owns its own copy of the value).
 * @param aName      property name to expose
 * @param aType      property value type
 * @param aInitValue initial value
 */
#define DBUSXX_PROPERTY_RW(aName, aType, aInitValue)                                        \
    static inline int _dbusxx_reg_prop_##aName = [] {                                        \
        Self::registry().push_back({                                                        \
            #aName,                                                                         \
            Self::sPath,                                                                    \
            Self::sIface,                                                                   \
            [](void* aBuilder, void* aObj) -> void {                                        \
                auto* builder = static_cast<::Dbusxx::Session::RegisterBuilder*>(aBuilder); \
                builder->addProperty<aType>(#aName, aInitValue, true);                      \
            }                                                                               \
        });                                                                                 \
        return 0;                                                                           \
    }();

}
#endif