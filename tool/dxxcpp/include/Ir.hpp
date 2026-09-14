#ifndef DXXCPP_IR_HPP
#define DXXCPP_IR_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


namespace Ir {
//! Type location
using TypeId = std::uint32_t;
//! Type name location
using NameIdx = std::uint32_t;

struct TypeBase {
    NameIdx name;
};

struct TypeVector {
    TypeId element;
};

struct TypeArray {
    TypeId element;
    std::uint32_t n;
};

struct TypeMap {
    TypeId key;
    TypeId value;
};

struct TypeStruct {
    std::uint32_t def;
};

struct TypeNode {
    std::variant<
        TypeBase,
        TypeVector,
        TypeArray,
        TypeMap,
        TypeStruct
    > kind;
};

struct Field {
    std::string name;
    TypeId type;
};
using Parameter = Field;

struct StructType {
    std::string name;
    std::vector<Field> fields;
};

struct AliasType {
    std::string name;
    TypeId target;
};

struct Method {
    std::string name;
    std::vector<Parameter> params;
    std::optional<TypeId> ret;
    std::optional<std::uint64_t> timeoutUsec;
    bool deprecated { false };
};

struct Signal {
    std::string name;
    std::vector<Parameter> params;
};

struct Property {
    std::string name;
    TypeId type;
    std::string defaultValue;
    bool readonly { false };
};

struct Interface {
    std::string name;
    std::vector<Method> methods;
    std::vector<Signal> signals;
    std::vector<Property> properties;
};

struct Root {
    std::string package;
    std::vector<TypeNode> types;
    std::vector<std::string> typeNames;
    std::vector<StructType> structs;
    std::vector<AliasType> aliases;
    std::vector<Interface> interfaces;

    //! Add base type name, return name idx
    NameIdx addBaseTypeName(std::string aName) {
        for (NameIdx i = 0; i < typeNames.size(); ++i) {
            if (typeNames[i] == aName) {
                return i;
            }
        }

        typeNames.push_back(std::move(aName));
        return static_cast<NameIdx>(typeNames.size() - 1);
    }

    //! Add type node
    TypeId addTypeNode(TypeNode aNode) {
        types.push_back(std::move(aNode));
        return static_cast<TypeId>(types.size() - 1);
    }

    //! Get type node by type id
    const TypeNode& type(TypeId aId) const {
        return types[aId];
    }

    //! Get base type name by name idx
    const std::string& nameOf(NameIdx aIdx) const {
        return typeNames[aIdx];
    }

    //! Get struct type by struct idx
    const StructType& structBy(std::uint32_t aIdx) const {
        return structs[aIdx];
    }

    //! Get leaf's type name
    const std::string& leafName(const TypeNode& aNode) const {
        if (auto* base = std::get_if<TypeBase>(&aNode.kind)) {
            return nameOf(base->name);
        }

        if (auto* st = std::get_if<TypeStruct>(&aNode.kind)) {
            return structBy(st->def).name;
        }

        static const std::string EMPTY;
        return EMPTY;
    }

};
}
#endif