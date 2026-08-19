
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
template<std::size_t I>
struct Probe {
    template<typename T>
   constexpr operator T() const noexcept;
};

template <typename T, std::size_t... Is>
constexpr auto probe_n(std::index_sequence<Is...>) noexcept
    -> decltype(T{ Probe<Is>{}... });

template <typename T, std::size_t N, typename = void>
struct fields_count : fields_count<T, N - 1> {};

template <typename T, std::size_t N>
struct fields_count<T, N,
    std::void_t<decltype(probe_n<T>(std::make_index_sequence<N>{}))>> {
    static constexpr std::size_t value = N;
};

template<typename T>
struct fields_count<T, 0, void> {
    static constexpr std::size_t value = 0;
};

template<typename T>
constexpr std::size_t fields_count_v = fields_count<T, 32>::value; 

//! ------------------------------------------------------------------
//! tie_as_tuple overloads are macro-generated so the field-count limit
//! can be raised without hand-writing each overload.
//! To support more than 20 fields, extend DBUSXX_DETAIL_FIELDS_N and
//! DBUSXX_DETAIL_TIE_TUPLE_N (and bump the static_assert below).
//! ------------------------------------------------------------------
//! Single-level ## is enough here: the right operand is always a literal (1..20),
//! which ## does not need to expand first. Keep the two-level CAT form only if
//! the field count ever comes from a macro (e.g. DBUSXX_DETAIL_FIELDS(MAX_FIELDS)).
#define DBUSXX_DETAIL_CAT(a, b) a##b

//! Comma-separated structured-binding names for N fields.
#define DBUSXX_DETAIL_FIELDS_1  a
#define DBUSXX_DETAIL_FIELDS_2  DBUSXX_DETAIL_FIELDS_1, b
#define DBUSXX_DETAIL_FIELDS_3  DBUSXX_DETAIL_FIELDS_2, c
#define DBUSXX_DETAIL_FIELDS_4  DBUSXX_DETAIL_FIELDS_3, d
#define DBUSXX_DETAIL_FIELDS_5  DBUSXX_DETAIL_FIELDS_4, e
#define DBUSXX_DETAIL_FIELDS_6  DBUSXX_DETAIL_FIELDS_5, f
#define DBUSXX_DETAIL_FIELDS_7  DBUSXX_DETAIL_FIELDS_6, g
#define DBUSXX_DETAIL_FIELDS_8  DBUSXX_DETAIL_FIELDS_7, h
#define DBUSXX_DETAIL_FIELDS_9  DBUSXX_DETAIL_FIELDS_8, i
#define DBUSXX_DETAIL_FIELDS_10 DBUSXX_DETAIL_FIELDS_9, j
#define DBUSXX_DETAIL_FIELDS_11 DBUSXX_DETAIL_FIELDS_10, k
#define DBUSXX_DETAIL_FIELDS_12 DBUSXX_DETAIL_FIELDS_11, l
#define DBUSXX_DETAIL_FIELDS_13 DBUSXX_DETAIL_FIELDS_12, m
#define DBUSXX_DETAIL_FIELDS_14 DBUSXX_DETAIL_FIELDS_13, n
#define DBUSXX_DETAIL_FIELDS_15 DBUSXX_DETAIL_FIELDS_14, o
#define DBUSXX_DETAIL_FIELDS_16 DBUSXX_DETAIL_FIELDS_15, p
#define DBUSXX_DETAIL_FIELDS_17 DBUSXX_DETAIL_FIELDS_16, q
#define DBUSXX_DETAIL_FIELDS_18 DBUSXX_DETAIL_FIELDS_17, r
#define DBUSXX_DETAIL_FIELDS_19 DBUSXX_DETAIL_FIELDS_18, s
#define DBUSXX_DETAIL_FIELDS_20 DBUSXX_DETAIL_FIELDS_19, t

#define DBUSXX_DETAIL_FIELDS(N) DBUSXX_DETAIL_CAT(DBUSXX_DETAIL_FIELDS_, N)

//! Generate one tie_as_tuple overload for a struct with N fields.
#define DBUSXX_DETAIL_TIE_AS_TUPLE(N)                                       \
    template <class T>                                                      \
    auto tie_as_tuple(T& v, std::integral_constant<std::size_t, N>)         \
        -> decltype(auto) {                                                 \
        auto& [DBUSXX_DETAIL_FIELDS(N)] = v;                                \
        return std::tie(DBUSXX_DETAIL_FIELDS(N));                           \
    }

//! Recursively emit overloads 1..N (invoke DBUSXX_DETAIL_TIE_TUPLE_<N>).
#define DBUSXX_DETAIL_TIE_TUPLE_1  DBUSXX_DETAIL_TIE_AS_TUPLE(1)
#define DBUSXX_DETAIL_TIE_TUPLE_2  DBUSXX_DETAIL_TIE_AS_TUPLE(2)  DBUSXX_DETAIL_TIE_TUPLE_1
#define DBUSXX_DETAIL_TIE_TUPLE_3  DBUSXX_DETAIL_TIE_AS_TUPLE(3)  DBUSXX_DETAIL_TIE_TUPLE_2
#define DBUSXX_DETAIL_TIE_TUPLE_4  DBUSXX_DETAIL_TIE_AS_TUPLE(4)  DBUSXX_DETAIL_TIE_TUPLE_3
#define DBUSXX_DETAIL_TIE_TUPLE_5  DBUSXX_DETAIL_TIE_AS_TUPLE(5)  DBUSXX_DETAIL_TIE_TUPLE_4
#define DBUSXX_DETAIL_TIE_TUPLE_6  DBUSXX_DETAIL_TIE_AS_TUPLE(6)  DBUSXX_DETAIL_TIE_TUPLE_5
#define DBUSXX_DETAIL_TIE_TUPLE_7  DBUSXX_DETAIL_TIE_AS_TUPLE(7)  DBUSXX_DETAIL_TIE_TUPLE_6
#define DBUSXX_DETAIL_TIE_TUPLE_8  DBUSXX_DETAIL_TIE_AS_TUPLE(8)  DBUSXX_DETAIL_TIE_TUPLE_7
#define DBUSXX_DETAIL_TIE_TUPLE_9  DBUSXX_DETAIL_TIE_AS_TUPLE(9)  DBUSXX_DETAIL_TIE_TUPLE_8
#define DBUSXX_DETAIL_TIE_TUPLE_10 DBUSXX_DETAIL_TIE_AS_TUPLE(10) DBUSXX_DETAIL_TIE_TUPLE_9
#define DBUSXX_DETAIL_TIE_TUPLE_11 DBUSXX_DETAIL_TIE_AS_TUPLE(11) DBUSXX_DETAIL_TIE_TUPLE_10
#define DBUSXX_DETAIL_TIE_TUPLE_12 DBUSXX_DETAIL_TIE_AS_TUPLE(12) DBUSXX_DETAIL_TIE_TUPLE_11
#define DBUSXX_DETAIL_TIE_TUPLE_13 DBUSXX_DETAIL_TIE_AS_TUPLE(13) DBUSXX_DETAIL_TIE_TUPLE_12
#define DBUSXX_DETAIL_TIE_TUPLE_14 DBUSXX_DETAIL_TIE_AS_TUPLE(14) DBUSXX_DETAIL_TIE_TUPLE_13
#define DBUSXX_DETAIL_TIE_TUPLE_15 DBUSXX_DETAIL_TIE_AS_TUPLE(15) DBUSXX_DETAIL_TIE_TUPLE_14
#define DBUSXX_DETAIL_TIE_TUPLE_16 DBUSXX_DETAIL_TIE_AS_TUPLE(16) DBUSXX_DETAIL_TIE_TUPLE_15
#define DBUSXX_DETAIL_TIE_TUPLE_17 DBUSXX_DETAIL_TIE_AS_TUPLE(17) DBUSXX_DETAIL_TIE_TUPLE_16
#define DBUSXX_DETAIL_TIE_TUPLE_18 DBUSXX_DETAIL_TIE_AS_TUPLE(18) DBUSXX_DETAIL_TIE_TUPLE_17
#define DBUSXX_DETAIL_TIE_TUPLE_19 DBUSXX_DETAIL_TIE_AS_TUPLE(19) DBUSXX_DETAIL_TIE_TUPLE_18
#define DBUSXX_DETAIL_TIE_TUPLE_20 DBUSXX_DETAIL_TIE_AS_TUPLE(20) DBUSXX_DETAIL_TIE_TUPLE_19

template <class T>
auto tie_as_tuple(T&, std::integral_constant<std::size_t, 0>) -> std::tuple<> {
    return {};
}

//! Expand the generated overloads (1..20 fields).
DBUSXX_DETAIL_TIE_TUPLE_20

template <class T>
auto tie_as_tuple(T& v) -> decltype(auto) {
    constexpr std::size_t N = fields_count_v<T>;
    static_assert(N <= 20,
        "tie_as_tuple supports aggregates with at most 20 fields; "
        "extend DBUSXX_DETAIL_FIELDS_/DBUSXX_DETAIL_TIE_TUPLE_ limits to raise it");
    return tie_as_tuple(v, std::integral_constant<std::size_t, N>{});
}

template <std::size_t I, class T>
using member_type = std::remove_reference_t<
    std::tuple_element_t<I, decltype(tie_as_tuple(std::declval<T&>()))>>;

template <class T, std::size_t... Is>
auto member_types_impl(std::index_sequence<Is...>)
    -> std::tuple<member_type<Is, T>...>;

template <class T>
using member_types = decltype(member_types_impl<T>(
    std::make_index_sequence<fields_count_v<T>>{}));

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
inline constexpr bool isStructV =
    std::is_aggregate_v<T> &&
    std::is_class_v<T> &&
    !isArrayV<T> &&
    !isVectorV<T> &&
    !isMapV<T> && 
    fields_count_v<T> > 0;

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
    else if constexpr (isStructV<R>) {
        auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
            std::string s;
            s.push_back('(');
            ((s += getSignature<Args>()), ...);
            s.push_back(')');
            return s;
        };

        return impl(static_cast<member_types<R>*>(nullptr));     
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
        || isMapV<R>
        || isStructV<R>;
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