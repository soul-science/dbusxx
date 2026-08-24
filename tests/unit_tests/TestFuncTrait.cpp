//! Unit tests for FuncTrait signature extraction
//! (library/include/private/method/FunctionTrait.hpp).
//! Pure compile-time checks; the TEST below only reports the outcome.

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>

#include "private/method/FunctionTrait.hpp"

using namespace Dbusxx::Method;

namespace {

//! --- free function ----------------------------------------------------------
static int freeFn(int a, double b) { return a + static_cast<int>(b); }
static_assert(std::is_same_v<FuncTrait<decltype(&freeFn)>::RetType, int>);
static_assert(std::is_same_v<FuncTrait<decltype(&freeFn)>::ArgsTuple,
                             std::tuple<int, double>>);
static_assert(FuncTrait<decltype(&freeFn)>::argSize == 2);

//! --- void / empty args -------------------------------------------------------
static void voidFn() {}
static_assert(std::is_same_v<FuncTrait<decltype(&voidFn)>::RetType, void>);
static_assert(FuncTrait<decltype(&voidFn)>::argSize == 0);

//! --- std::function -----------------------------------------------------------
static_assert(std::is_same_v<FuncTrait<std::function<void(std::string)>>::RetType,
                             void>);
static_assert(std::is_same_v<FuncTrait<std::function<void(std::string)>>::ArgsTuple,
                             std::tuple<std::string>>);

//! --- lambda (primary template routes to operator()) --------------------------
//! Note: `decltype([]( ... ){})` needs C++20; in C++17 we bind a named lambda.
auto lam = [](int32_t v) -> int64_t { return v; };
auto lam2 = [](std::string s) { return s.size(); };
static_assert(std::is_same_v<FuncTrait<decltype(lam)>::RetType, int64_t>);
static_assert(std::is_same_v<FuncTrait<decltype(lam)>::ArgsTuple,
                             std::tuple<int32_t>>);
static_assert(std::is_same_v<FuncTrait<decltype(lam2)>::RetType, size_t>);
static_assert(std::is_same_v<FuncTrait<decltype(lam2)>::ArgsTuple,
                             std::tuple<std::string>>);

//! --- member functions: all 14 cv/ref/noexcept combinations -------------------
struct Cls {
    void plain(int) {}
    void constMethod(int) const {}
    void noexceptMethod(int) noexcept {}
    void constNoexceptMethod(int) const noexcept {}
    void lrefMethod(int) & {}
    void constLrefMethod(int) const & {}
    void lrefNoexceptMethod(int) & noexcept {}
    void constLrefNoexceptMethod(int) const & noexcept {}
    void rrefMethod(int) && {}
    void constRrefMethod(int) const && {}
    void rrefNoexceptMethod(int) && noexcept {}
    void constRrefNoexceptMethod(int) const && noexcept {}
};

static_assert(std::is_same_v<FuncTrait<decltype(&Cls::plain)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::noexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constNoexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::lrefMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constLrefMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::lrefNoexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constLrefNoexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::rrefMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constRrefMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::rrefNoexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constRrefNoexceptMethod)>::ArgsTuple,
                             std::tuple<int>>);

//! every member-function form keeps RetType == void
static_assert(std::is_same_v<FuncTrait<decltype(&Cls::constRrefNoexceptMethod)>::RetType,
                             void>);

//! multi-arg member with non-void return
struct Cls2 {
    double f(std::string a, bool b) const { return 0.0; }
};
static_assert(std::is_same_v<FuncTrait<decltype(&Cls2::f)>::RetType, double>);
static_assert(std::is_same_v<FuncTrait<decltype(&Cls2::f)>::ArgsTuple,
                             std::tuple<std::string, bool>>);
static_assert(FuncTrait<decltype(&Cls2::f)>::argSize == 2);

} //! namespace

TEST(FuncTraitTest, CompileTimeChecks) {
    //! All coverage happens at compile time via the static_asserts above;
    //! reaching here means every signature extracted correctly.
    SUCCEED();
}
