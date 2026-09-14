#ifndef DXXCPP_AST_HPP
#define DXXCPP_AST_HPP

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <unordered_set>

namespace Ast {
constexpr std::string_view VECTOR_STR { "vector" };
constexpr std::string_view MAP_STR { "map" };

struct Loc {
    std::size_t line { 0 };
    std::size_t col { 0 };
};

struct Package {
    std::string name;
    Loc loc;
};

/**
 * @brief Type expression node ("mute" syntax tree, without type classification)
 *
 *   int32                        -> { "int32", {} }
 *   Point / ConfigMap            -> { "Point"/"ConfigMap", {} }
 *   vector<T>                    -> { "vector", { T } }
 *   map<K,V>                     -> { "map",    { K, V } }
 *   array<T,N>                   -> { "array",  { T, N } }
 *
 * The classification and validation of basic type sets,
 *   container keywords, operands, dict keys,
 *   etc. all belong to Sema (output Ir:: Type).
 */
struct Type {
    std::string name;
    std::vector<Type> templateType;
    Loc loc;

    bool isVector() {
        return name == VECTOR_STR;
    }

    bool isMap() {
        return name == MAP_STR;
    }

};


struct Field {
    Type type;
    std::string name;
    Loc loc;
};
using Parameter = Field;

/**
 * @brief Indicate a struct
 * 
 * Ex:
 *   struct Point {
 *       int32 x;
 *       int32 y;
 *   };
 */
struct StructType {
    std::string name;
    std::vector<Field> fields;
    Loc loc;
};

/**
 * @brief Indicate a alias(using)
 * 
 * Ex: using ConfigMap = map<string, string>;
 */
struct AliasType {
    std::string name;
    Type targetType;
    Loc loc;
};

/**
 * @brief Indicate an annotation
 * 
 * Ex:
 *   - @readonly       -> name="readonly", value=nullopt
 *   - @timeout(3000)  -> name="timeout", value="3000"
 *   - @deprecated     -> name="deprecated", value=nullopt
 */
struct Annotation {
    std::string name;
    std::optional<std::string> value;
    Loc loc;
};

/**
 * @brief Indicate a method
 * 
 * Ex:
 *   - @deprecated method add(int32 a, int32 b) -> int32;
 *   - @timeout(3000) method getConfig() -> ConfigMap;
 *   - method notify(string msg);
 */
struct Method {
    std::string name;
    std::vector<Parameter> params;
    std::optional<Type> retType;
    std::vector<Annotation> annotations;
    Loc loc;

    bool hasReturn() const {
        return retType.has_value();
    }

    bool isVoidReturn() const {
        return !retType.has_value();
    }
};

/**
 * @brief Indicate a signal
 * 
 * Ex:
 *   - signal valueChanged(int32 oldVal, int32 newVal);
 */
struct Signal {
    std::string name;
    std::vector<Parameter> params;
    std::vector<Annotation> annotations;
    Loc loc;
};

/**
 * @brief Indicate a property
 * 
 * Ex:
 *   - @readonly property version -> int32{1};
 *   - property label -> string{"default"};
 */
struct Property {
    std::string name;
    Type type;
    //! Initial value original text
    std::string defaultValue;
    std::vector<Annotation> annotations;
    Loc loc;
};

/**
 * @brief Indicate an interface
 * 
 * Ex:
 *   interface Calculator {
 *       method add(...) -> int32;
 *       signal valueChanged(...);
 *       property version -> string{"1.0.0"};
 *   };
 */
struct Interface {
    std::string name;
    std::vector<Method> methods;
    std::vector<Signal> signals;
    std::vector<Property> properties;
    Loc loc;

    std::string fullName(const std::string& package) const {
        return package + "." + name;
    }

    std::vector<std::string> getMethodNames() const {
        std::vector<std::string> names;
        for (const auto& m : methods) {
            names.push_back(m.name);
        }

        return names;
    }
};

/**
 * @brief Indicate a root of .dxx
 * 
 * Contains:
 *   - package:     package like "com.example.app"
 *   - structs:     struct list
 *   - alias:       alias list
 *   - interfaces:  interfaces list
 */
struct Root {
    Package package;
    std::vector<StructType> structs;
    std::vector<AliasType> alias;
    std::vector<Interface> interfaces;

    std::string path() const {
        std::string path = "/";
        std::size_t start = 0;
        std::size_t end;
        std::string name = package.name;
        while ((end = name.find('.', start)) != std::string::npos) {
            path += name.substr(start, end - start) + "/";
            start = end + 1;
        }

        path += name.substr(start);
        return path;
    }

    const StructType* findStruct(const std::string& aName) const {
        for (const auto& s : structs) {
            if (s.name == aName) {
                return &s;
            }
        }
        return nullptr;
    }

    const AliasType* findAlias(const std::string& aName) const {
        for (const auto& u : alias) {
            if (u.name == aName) {
                return &u;
            }
        }
        return nullptr;
    }
};
}

#endif