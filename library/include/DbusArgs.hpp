
#ifndef SSDBUS_DBUS_ARGS_HPP
#define SSDBUS_DBUS_ARGS_HPP

#include <string>
#include <type_traits>

namespace SSDbus {

template<typename T>
struct DbusTypeSignature {
    static_assert(sizeof(T) == 0, "Unsupported Dbus type");
    static constexpr const char* sig = "";
};

template<>
struct DbusTypeSignature<int8_t> {
    static constexpr const char* value = "i";
};

template<>
struct DbusTypeSignature<uint8_t> {
    static constexpr const char* value = "u";
};

template<>
struct DbusTypeSignature<int16_t> {
    static constexpr const char* value = "i";
};

template<>
struct DbusTypeSignature<uint16_t> {
    static constexpr const char* value = "u";
};

template<>
struct DbusTypeSignature<int32_t> {
    static constexpr const char* value = "i";
};

template<>
struct DbusTypeSignature<uint32_t> {
    static constexpr const char* value = "u";
};

template<>
struct DbusTypeSignature<int64_t> {
    static constexpr const char* value = "x";
};

template<>
struct DbusTypeSignature<uint64_t> {
    static constexpr const char* value = "t";
};

//! double

template<>
struct DbusTypeSignature<double> {
    static constexpr const char* value = "d";
};

template<>
struct DbusTypeSignature<float> {
    static constexpr const char* value = "d";
};

//! bool
template<>
struct DbusTypeSignature<bool> {
    static constexpr const char* value = "b";
};

//! char
template<>
struct DbusTypeSignature<const char*> {
    static constexpr const char* value = "s";
};

template<>
struct DbusTypeSignature<std::string> {
    static constexpr const char* value = "s";
};

//! void
template<>
struct DbusTypeSignature<void> {
    static constexpr const char* value = "";
};

template<typename T>
constexpr const char* getSignature() {
    return DbusTypeSignature<std::decay_t<T>>::value;
}

}

#endif