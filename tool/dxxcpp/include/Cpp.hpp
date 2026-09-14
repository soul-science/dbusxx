#ifndef DXXCPP_CPP_HPP
#define DXXCPP_CPP_HPP

#include <string>
#include <unordered_set>


namespace Cpp {
//! Determine if the name is a C++ keyword(identifier)
inline bool isKeyword(const std::string& aName) {
    static const std::unordered_set<std::string> KEYWORDS = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto",
        "bitand", "bitor", "bool", "break",
        "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr",
        "constinit", "const_cast", "continue", "co_await", "co_return",
        "co_yield", "decltype", "default", "delete", "do", "double",
        "dynamic_cast", "else", "enum", "explicit", "export", "extern",
        "false", "float", "for", "friend", "goto", "if", "inline", "int",
        "long", "mutable", "namespace", "new", "noexcept", "not",
        "not_eq", "nullptr", "operator", "or", "or_eq", "private",
        "protected", "public", "register", "reinterpret_cast", "requires",
        "return", "short", "signed", "sizeof", "static", "static_assert",
        "static_cast", "struct", "switch", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid",
        "typename", "union", "unsigned", "using", "virtual", "void",
        "volatile", "wchar_t", "while", "xor", "xor_eq"
    };

    return KEYWORDS.count(aName) > 0;
}

inline std::string ident(const std::string& aName) {
    return isKeyword(aName) ? (aName + "_") : aName;
}
} // namespace Cpp

#endif // DXXCPP_CPP_HPP
