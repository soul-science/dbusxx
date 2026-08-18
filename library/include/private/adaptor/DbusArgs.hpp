
#ifndef DBUSXX_DBUS_ARGS_HPP
#define DBUSXX_DBUS_ARGS_HPP

#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>


namespace Dbusxx {
template<typename T>
struct isArray : std::false_type {};

template<typename T, size_t N>
struct isArray<std::array<T, N>> : std::true_type {};

template<typename T>
inline constexpr bool isArrayV = isArray<T>::value;

template<typename T, template<typename...> class Template>
struct isSpecializationOf : std::false_type {};

template<template<typename...> class Template, typename... Args>
struct isSpecializationOf<Template<Args...>, Template> : std::true_type {};

template<typename T>
inline constexpr bool isVectorV = isSpecializationOf<T, std::vector>::value;

template<typename T>
inline constexpr bool isMapV = isSpecializationOf<T, std::map>::value
    || isSpecializationOf<T, std::unordered_map>::value;

template<typename T>
struct BasicSignature {
    static_assert(sizeof(T) == 0, "Unsupported Dbus type");
    static constexpr char value {'\0'};
};

template<>
struct BasicSignature<int8_t> {
    //! Use 'y' (for uint8_t)
    //! Ensure that the byte count is consistent with int8_t
    static constexpr char value = 'y';
};

template<>
struct BasicSignature<uint8_t> {
    static constexpr char value = 'y';
};

template<>
struct BasicSignature<int16_t> {
    static constexpr char value = 'n';
};

template<>
struct BasicSignature<uint16_t> {
    static constexpr char value = 'q';
};

template<>
struct BasicSignature<int32_t> {
    static constexpr char value = 'i';
};

template<>
struct BasicSignature<uint32_t> {
    static constexpr char value = 'u';
};

template<>
struct BasicSignature<int64_t> {
    static constexpr char value = 'x';
};

template<>
struct BasicSignature<uint64_t> {
    static constexpr char value = 't';
};

//! double
template<>
struct BasicSignature<double> {
    static constexpr char value = 'd';
};

template<>
struct BasicSignature<float> {
    static constexpr char value = 'd';
};

//! bool
template<>
struct BasicSignature<bool> {
    static constexpr char value = 'b';
};

//! char
template<>
struct BasicSignature<const char*> {
    static constexpr char value = 's';
};

template<>
struct BasicSignature<char*> {
    static constexpr char value = 's';
};

template<>
struct BasicSignature<std::string> {
    static constexpr char value = 's';
};

template<>
struct BasicSignature<std::string_view> {
    static constexpr char value = 's';
};

//! void
template<>
struct BasicSignature<void> {
    static constexpr char value = '\0';
};

template<typename T>
std::string getSignature() {
    using R = std::decay_t<T>;
    if constexpr (std::is_same_v<R, void>) {
        return "";
    }
    else if constexpr (isVectorV<R> || isArrayV<R>) {
        return "a" + getSignature<typename R::value_type>();
    }
    else if constexpr (isMapV<R>) {
        std::string s;
        s.push_back('a');
        s.push_back('{');
        s.append(getSignature<typename R::key_type>());
        s.append(getSignature<typename R::mapped_type>());
        s.push_back('}');
        return s;
    }
    else {
        return std::string(1, BasicSignature<R>::value);
    }
}

template<typename T>
constexpr bool isValidArgs() {
    using R = std::decay_t<T>;
    return std::is_integral_v<R>
        || std::is_floating_point_v<R>
        || std::is_same_v<R, bool>
        || std::is_same_v<R, const char*>
        || std::is_same_v<R, char*>
        || std::is_same_v<R, std::string>
        || std::is_same_v<R, std::string_view>
        || isVectorV<R>
        || isArrayV<R>
        || isMapV<R>;
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