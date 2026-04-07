
#ifndef SSDBUS_DBUS_ARGS_HPP
#define SSDBUS_DBUS_ARGS_HPP

#include <string>
#include <string_view>
#include <type_traits>

namespace SSDbus {

template<typename T>
struct DbusTypeSignature {
    static_assert(sizeof(T) == 0, "Unsupported Dbus type");
    static constexpr char value {'\0'};
};

template<>
struct DbusTypeSignature<int8_t> {
    static constexpr char value = 'n';
};

template<>
struct DbusTypeSignature<uint8_t> {
    static constexpr char value = 'y';
};

template<>
struct DbusTypeSignature<int16_t> {
    static constexpr char value = 'n';
};

template<>
struct DbusTypeSignature<uint16_t> {
    static constexpr char value = 'q';
};

template<>
struct DbusTypeSignature<int32_t> {
    static constexpr char value = 'i';
};

template<>
struct DbusTypeSignature<uint32_t> {
    static constexpr char value = 'u';
};

template<>
struct DbusTypeSignature<int64_t> {
    static constexpr char value = 'x';
};

template<>
struct DbusTypeSignature<uint64_t> {
    static constexpr char value = 't';
};

//! double
template<>
struct DbusTypeSignature<double> {
    static constexpr char value = 'd';
};

template<>
struct DbusTypeSignature<float> {
    static constexpr char value = 'd';
};

//! bool
template<>
struct DbusTypeSignature<bool> {
    static constexpr char value = 'b';
};

//! char
template<>
struct DbusTypeSignature<const char*> {
    static constexpr char value = 's';
};

template<>
struct DbusTypeSignature<std::string> {
    static constexpr char value = 's';
};

template<>
struct DbusTypeSignature<std::string_view> {
    static constexpr char value = 's';
};

//! void
template<>
struct DbusTypeSignature<void> {
    static constexpr char value = '\0';
};

template<typename T>
constexpr char getSignature() {
    return DbusTypeSignature<std::decay_t<T>>::value;
}

template<typename T>
struct ArgTypeAdaptor {
    using type = T;
};

template<>
struct ArgTypeAdaptor<float> {
    using type = double;
};

template<>
struct ArgTypeAdaptor<std::string_view> {
    using type = const char*;
};

}

#endif