#ifndef DBUSXX_FUNCTION_TRAIT_HPP
#define DBUSXX_FUNCTION_TRAIT_HPP

#include <functional>
#include <tuple>


namespace Dbusxx {
namespace Method {
template<typename F>
struct FuncTrait : FuncTrait<decltype(&F::operator())> {};

template<typename Ret, typename... Args>
struct FuncTrait<Ret(*)(Args...)> {
    using RetType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t argSize = sizeof...(Args);
};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...)> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const noexcept> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) noexcept> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) &> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const &> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) & noexcept> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const & noexcept> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) &&> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const &&> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...)  && noexcept> : FuncTrait<Ret(*)(Args...)> {};
template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...)  const && noexcept> : FuncTrait<Ret(*)(Args...)> {};

template<typename Ret, typename... Args>
struct FuncTrait<std::function<Ret(Args...)>> : FuncTrait<Ret(*)(Args...)> {};

}
}

#endif