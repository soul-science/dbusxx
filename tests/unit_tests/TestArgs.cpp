//! Unit tests for the type-extraction layer (library/include/Args.hpp).
//! Mostly compile-time static_asserts; the runtime helpers (getSignature /
//! tieAsTuple) are exercised through Google Test.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Args.hpp"

using namespace Dbusxx;

namespace {

//! --- test aggregate structs --------------------------------------------------
struct Empty {};
struct One { int32_t a; };
struct Point { int32_t x; int32_t y; };
struct Person { std::string name; int32_t age; };
struct WithArray { int32_t data[2]; };      //! native array member
struct Point20 {
    int32_t f01, f02, f03, f04, f05, f06, f07, f08, f09, f10;
    int32_t f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
};

//! Aggregate with a UnixFd member — aggregate-ness only depends on the outer
//! class (no user ctors / virtuals / private members), NOT on member types.
struct WithFd {
    UnixFd      fd;
    std::string path;
    int32_t     flags;
};

} //! namespace

//! --- BasicSignature: every primitive maps to its D-Bus type char ------------
static_assert(BasicSignature<int8_t>::value == 'y');
static_assert(BasicSignature<uint8_t>::value == 'y');
static_assert(BasicSignature<int16_t>::value == 'n');
static_assert(BasicSignature<uint16_t>::value == 'q');
static_assert(BasicSignature<int32_t>::value == 'i');
static_assert(BasicSignature<uint32_t>::value == 'u');
static_assert(BasicSignature<int64_t>::value == 'x');
static_assert(BasicSignature<uint64_t>::value == 't');
static_assert(BasicSignature<double>::value == 'd');
static_assert(BasicSignature<float>::value == 'd');       //! float → double
static_assert(BasicSignature<bool>::value == 'b');
static_assert(BasicSignature<const char*>::value == 's');
static_assert(BasicSignature<char*>::value == 's');
static_assert(BasicSignature<std::string>::value == 's');
static_assert(BasicSignature<std::string_view>::value == 's');
static_assert(BasicSignature<void>::value == '\0');

//! --- trait detection --------------------------------------------------------
static_assert(isArrayV<std::array<int32_t, 4>>);
static_assert(!isArrayV<std::vector<int32_t>>);
static_assert(isVectorV<std::vector<int32_t>>);
static_assert(!isVectorV<std::array<int32_t, 4>>);
static_assert(isMapV<std::map<std::string, int32_t>>);
static_assert(isMapV<std::unordered_map<int32_t, double>>);
static_assert(!isMapV<std::vector<int32_t>>);
static_assert(isStructV<Point>);
static_assert(isStructV<Person>);
static_assert(!isStructV<std::string>);
static_assert(!isStructV<std::vector<int32_t>>);
static_assert(!isStructV<std::array<int32_t, 4>>);
static_assert(!isStructV<std::map<std::string, int32_t>>);
static_assert(!isStructV<Empty>);
//! NB: a native-array member is currently *counted* as 2 by the Probe
//! machinery on libstdc++ (it fills the array), so WithArray is seen as a
//! struct. Its getSignature()/tieAsTuple() fail to compile — that is an
//! existing library limitation (see note in plan.md §17), not tested here.
static_assert(isStructV<WithArray>);

//! --- memberCountV -----------------------------------------------------------
static_assert(memberCountV<Empty> == 0);
static_assert(memberCountV<One> == 1);
static_assert(memberCountV<Point> == 2);
static_assert(memberCountV<Person> == 2);
static_assert(memberCountV<Point20> == 20);
static_assert(memberCountV<WithArray> == 2);   //! native array counted as 2

//! --- std::pair / std::tuple (libstdc++ on GCC 11) ---------------------------
//! On this implementation std::pair is NOT an aggregate (it has user-provided
//! constructors via _PCC), so it is correctly excluded. std::tuple likewise.
static_assert(!isStructV<std::pair<int32_t, std::string>>);
static_assert(memberCountV<std::pair<int32_t, std::string>> == 0);
static_assert(!isStructV<std::tuple<int32_t, int32_t>>);
static_assert(memberCountV<std::tuple<int32_t, int32_t>> == 0);

//! --- memberTypesT -----------------------------------------------------------
static_assert(std::is_same_v<memberTypeT<0, Point>, int32_t>);
static_assert(std::is_same_v<memberTypeT<1, Point>, int32_t>);
static_assert(std::is_same_v<memberTypeT<0, Person>, std::string>);
static_assert(std::is_same_v<memberTypeT<1, Person>, int32_t>);
static_assert(std::is_same_v<memberTypesT<Point>, std::tuple<int32_t, int32_t>>);

//! --- isValidArg / isValidArgs ------------------------------------------------
static_assert(isValidArg<int8_t>());
static_assert(isValidArg<int32_t>());
static_assert(isValidArg<uint64_t>());
static_assert(isValidArg<float>());
static_assert(isValidArg<double>());
static_assert(isValidArg<bool>());
static_assert(isValidArg<std::string>());
static_assert(isValidArg<std::string_view>());
static_assert(isValidArg<const char*>());
static_assert(isValidArg<std::vector<int32_t>>());
static_assert(isValidArg<std::vector<std::vector<std::string>>>());
static_assert(isValidArg<std::array<double, 3>>());
static_assert(isValidArg<std::map<std::string, int32_t>>());
static_assert(isValidArg<std::map<std::string, std::vector<int32_t>>>());
static_assert(isValidArg<Point>());
static_assert(isValidArg<std::vector<Point>>());
static_assert(isValidArg<std::map<std::string, Point>>());
static_assert(!isValidArg<void>());
static_assert(!isValidArg<std::tuple<int32_t, int32_t>>());

static_assert(isValidArgs<int32_t, std::string, Point>());
static_assert(!isValidArgs<int32_t, void>());
static_assert(isValidArgs<>());

//! --- UnixFd -----------------------------------------------------------------
static_assert(BasicSignature<UnixFd>::value == 'h');
static_assert(isValidArg<UnixFd>());
static_assert(isValidArg<std::vector<UnixFd>>());
static_assert(!isStructV<UnixFd>);          //! user-provided ctor → non-aggregate
static_assert(memberCountV<UnixFd> == 0);

//! --- struct containing a UnixFd member -------------------------------------
//! UnixFd itself is not an aggregate, but the *outer* aggregate can still hold
//! it as a member; the library then maps it to (hsi).
static_assert(isStructV<WithFd>);
static_assert(memberCountV<WithFd> == 3);
static_assert(std::is_same_v<memberTypeT<0, WithFd>, UnixFd>);
static_assert(std::is_same_v<memberTypeT<1, WithFd>, std::string>);
static_assert(isValidArg<WithFd>());
static_assert(isValidArg<std::vector<WithFd>>());
static_assert(isValidArg<std::map<std::string, WithFd>>());

//! --- ArgTypeAdaptor ---------------------------------------------------------
static_assert(std::is_same_v<ArgTypeAdaptor<float>::type, double>);
static_assert(std::is_same_v<ArgTypeAdaptor<std::string_view>::type, const char*>);
static_assert(std::is_same_v<ArgTypeAdaptor<int32_t>::type, int32_t>);

//! --- getSignature (runtime) --------------------------------------------------
TEST(ArgsTest, GetSignatureBasic) {
    EXPECT_STREQ(getSignature<int8_t>().c_str(), "y");
    EXPECT_STREQ(getSignature<uint8_t>().c_str(), "y");
    EXPECT_STREQ(getSignature<int16_t>().c_str(), "n");
    EXPECT_STREQ(getSignature<uint16_t>().c_str(), "q");
    EXPECT_STREQ(getSignature<int32_t>().c_str(), "i");
    EXPECT_STREQ(getSignature<uint32_t>().c_str(), "u");
    EXPECT_STREQ(getSignature<int64_t>().c_str(), "x");
    EXPECT_STREQ(getSignature<uint64_t>().c_str(), "t");
    EXPECT_STREQ(getSignature<double>().c_str(), "d");
    EXPECT_STREQ(getSignature<float>().c_str(), "d");
    EXPECT_STREQ(getSignature<bool>().c_str(), "b");
    EXPECT_STREQ(getSignature<std::string>().c_str(), "s");
    EXPECT_STREQ(getSignature<std::string_view>().c_str(), "s");
    EXPECT_STREQ(getSignature<const char*>().c_str(), "s");
    EXPECT_STREQ(getSignature<void>().c_str(), "");
}

TEST(ArgsTest, GetSignatureContainers) {
    //! Use locals for any template arg that contains a comma.
    EXPECT_STREQ(getSignature<std::vector<int32_t>>().c_str(), "ai");
    EXPECT_STREQ(getSignature<std::vector<std::vector<std::string>>>().c_str(), "aas");
    const std::string arr = getSignature<std::array<double, 3>>();
    EXPECT_STREQ(arr.c_str(), "ad");
    const std::string m1 = getSignature<std::map<std::string, int32_t>>();
    EXPECT_STREQ(m1.c_str(), "a{si}");
    const std::string m2 = getSignature<std::unordered_map<int32_t, double>>();
    EXPECT_STREQ(m2.c_str(), "a{id}");
    const std::string m3 =
        getSignature<std::map<std::string, std::vector<int32_t>>>();
    EXPECT_STREQ(m3.c_str(), "a{sai}");
}

TEST(ArgsTest, GetSignatureStruct) {
    EXPECT_STREQ(getSignature<Point>().c_str(), "(ii)");
    EXPECT_STREQ(getSignature<Person>().c_str(), "(si)");
    EXPECT_STREQ(getSignature<std::vector<Point>>().c_str(), "a(ii)");

    const std::string mp = getSignature<std::map<std::string, Point>>();
    EXPECT_STREQ(mp.c_str(), "a{s(ii)}");

    //! nested struct
    struct Nested {
        Point p;
        std::string tag;
    };
    EXPECT_STREQ(getSignature<Nested>().c_str(), "((ii)s)");
}

TEST(ArgsTest, TieAsTuple) {
    Point p{10, 20};
    auto t = tieAsTuple(p);
    static_assert(std::tuple_size_v<decltype(t)> == 2);
    static_assert(std::is_same_v<std::tuple_element_t<0, decltype(t)>, int32_t&>);
    EXPECT_EQ(std::get<0>(t), 10);
    EXPECT_EQ(std::get<1>(t), 20);
}

TEST(ArgsTest, GetSignatureUnixFd) {
    EXPECT_STREQ(getSignature<UnixFd>().c_str(), "h");
    EXPECT_STREQ(getSignature<std::vector<UnixFd>>().c_str(), "ah");
    //! struct with an fd member → (hsi)
    EXPECT_STREQ(getSignature<WithFd>().c_str(), "(hsi)");
    EXPECT_STREQ(getSignature<std::vector<WithFd>>().c_str(), "a(hsi)");
}
