//! AST construction helpers for tests (dumb-tree types + member builders),
//! so that the test files do not repeat them
#ifndef DXXCPP_TEST_ASTBUILDER_HPP
#define DXXCPP_TEST_ASTBUILDER_HPP


#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Ast.hpp"


namespace tb {
inline Ast::Type base(std::string aName) {
    return Ast::Type{ std::move(aName), {}, Ast::Loc{} };
}

inline Ast::Type named(std::string aName) {
    return Ast::Type{ std::move(aName), {}, Ast::Loc{} };
}

inline Ast::Type vect(Ast::Type aElement) {
    return Ast::Type{ "vector", { std::move(aElement) }, Ast::Loc{} };
}

inline Ast::Type array(Ast::Type aElement, std::uint32_t aSize) {
    return Ast::Type{
        "array",
        {
            std::move(aElement),
            Ast::Type{ std::to_string(aSize), {}, Ast::Loc{} }
        },
        Ast::Loc{}
    };  // N is a number leaf
}

inline Ast::Type mp(Ast::Type aKey, Ast::Type aValue) {
    return Ast::Type{ "map", { std::move(aKey), std::move(aValue) }, Ast::Loc{} };
}

inline Ast::Field field(Ast::Type aType, std::string aName) {
    Ast::Field aField;
    aField.type = std::move(aType);
    aField.name = std::move(aName);
    return aField;
}

inline Ast::Annotation ann(std::string aName,
  std::optional<std::string> aValue = std::nullopt) {
    Ast::Annotation aAnn;
    aAnn.name = std::move(aName);
    aAnn.value = std::move(aValue);
    return aAnn;
}

inline Ast::Method method(std::string aName, std::vector<Ast::Parameter> aParams,
  std::optional<Ast::Type> aRet = std::nullopt, std::vector<Ast::Annotation> aAnns = {}) {
    Ast::Method aMethod;
    aMethod.name = std::move(aName);
    aMethod.params = std::move(aParams);
    aMethod.retType = std::move(aRet);
    aMethod.annotations = std::move(aAnns);
    return aMethod;
}

inline Ast::Signal signal(std::string aName, std::vector<Ast::Parameter> aParams,
  std::vector<Ast::Annotation> aAnns = {}) {
    Ast::Signal aSignal;
    aSignal.name = std::move(aName);
    aSignal.params = std::move(aParams);
    aSignal.annotations = std::move(aAnns);
    return aSignal;
}

inline Ast::Property property(std::string aName, Ast::Type aType,
  std::string aDefault, std::vector<Ast::Annotation> aAnns = {}) {
    Ast::Property aProperty;
    aProperty.name = std::move(aName);
    aProperty.type = std::move(aType);
    aProperty.defaultValue = std::move(aDefault);
    aProperty.annotations = std::move(aAnns);
    return aProperty;
}

} // namespace tb
#endif
