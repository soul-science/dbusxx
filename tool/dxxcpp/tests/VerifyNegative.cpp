//! Negative cases: hand-built invalid AST -> Sema::analyze must report errors
//! and hit the expected messages. Standing regression entry for validation.
#include <iostream>
#include <string>
#include <vector>

#include "Ast.hpp"
#include "AstBuilder.hpp"
#include "Sema.hpp"


using namespace Ast;


namespace {
struct Case {
    const char* name;
    std::vector<std::string> needles;   // error messages must contain these parts
    Ast::Root (*make)();
};

int gFailures = 0;

Ast::Root pkg() {
    Ast::Root aRoot;
    aRoot.package.name = "p.q";
    return aRoot;
}

// struct A { A a; }                  -- self-inclusion by value
Ast::Root selfDirect() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(named("A"), "a") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// struct A { vector<A> v; }          -- self-reference through a container
Ast::Root selfViaVector() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(vect(named("A")), "v") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// struct A { B b; } struct B { A a; } -- mutual inclusion
Ast::Root selfMutual() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::StructType aStructType;
        aStructType.name = "A";
        aStructType.fields = { field(named("B"), "b") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        Ast::StructType aStructType;
        aStructType.name = "B";
        aStructType.fields = { field(named("A"), "a") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    return aRoot;
}

// using A = B; using B = A;          -- alias cycle
Ast::Root aliasCycle() {
    using namespace tb;
    auto aRoot = pkg();
    aRoot.alias.push_back(AliasType{ "A", named("B"), {} });
    aRoot.alias.push_back(AliasType{ "B", named("A"), {} });
    return aRoot;
}

// struct P { int32 x; }  +  map<P,int32>  -- map key is a struct
Ast::Root mapKeyStruct() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::StructType aStructType;
        aStructType.name = "P";
        aStructType.fields = { field(base("int32"), "x") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        Ast::StructType aStructType;
        aStructType.name = "Holder";
        aStructType.fields = { field(mp(named("P"), base("int32")), "m") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    return aRoot;
}

// unknown type
Ast::Root unknownType() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(named("Nope"), "a") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// struct Empty {};          -- an empty struct has no D-Bus signature
Ast::Root emptyStruct() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "Empty";
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// struct A { B b; } struct B { int32 x; } -- fields are by value, B comes first
Ast::Root forwardStructRef() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::StructType aStructType;
        aStructType.name = "A";
        aStructType.fields = { field(named("B"), "b") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        Ast::StructType aStructType;
        aStructType.name = "B";
        aStructType.fields = { field(base("int32"), "x") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    return aRoot;
}

// array<int32, 0>
Ast::Root arrayZero() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(array(base("int32"), 0), "a") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// map<int32> (wrong arity)
Ast::Root mapArity() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(Ast::Type{ "map", { base("int32") }, {} }, "a") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// two methods with the same name
Ast::Root dupMember() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}));
    aInterface.methods.push_back(method("f", {}));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// a method and a property share one member namespace
Ast::Root dupMemberMethodProp() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("val", {}));
    aInterface.properties.push_back(property("val", base("int32"), "0"));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// duplicate field x in struct A
Ast::Root dupField() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(base("int32"), "x"), field(base("string"), "x") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// a struct and an alias share a name
Ast::Root dupType() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::StructType aStructType;
        aStructType.name = "A";
        aStructType.fields = { field(base("int32"), "x") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    aRoot.alias.push_back(AliasType{ "A", base("int32"), {} });
    return aRoot;
}

// struct name takes a reserved type name
Ast::Root reservedName() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "int32";
    aStructType.fields = { field(base("int32"), "x") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// @timeout used on a property
Ast::Root timeoutOnProperty() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.properties.push_back(property("x", base("int32"), "0", { ann("timeout", "3000") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// unknown annotation
Ast::Root unknownAnnotation() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}, std::nullopt, { ann("weird") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// member name collides with a C++ keyword (used as an identifier by codegen)
Ast::Root keywordMember() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("new", {}, std::nullopt, {}));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// interface name collides with a C++ keyword (used as a class-name prefix)
Ast::Root keywordIface() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "class";
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @readonly used on a method
Ast::Root readonlyOnMethod() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}, std::nullopt, { ann("readonly") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @timeout(0) -> not a positive integer
Ast::Root timeoutZero() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}, std::nullopt, { ann("timeout", "0") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @timeout(abc) -> not a number at all
Ast::Root timeoutNotANumber() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}, std::nullopt, { ann("timeout", "abc") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @readonly("yes") on a property -> takes no value
Ast::Root readonlyWithValue() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.properties.push_back(property("p", base("int32"), "{0}",
        { ann("readonly", "yes") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @deprecated("1") -> takes no value
Ast::Root deprecatedWithValue() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}, std::nullopt, { ann("deprecated", "1") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// @sync used on a property
Ast::Root syncOnProperty() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.properties.push_back(property("p", base("int32"), "{0}", { ann("sync") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// the same annotation twice
Ast::Root dupAnnotation() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.properties.push_back(property("p", base("int32"), "{0}",
        { ann("readonly"), ann("readonly") }));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// vector<int32, int32> -> vector<T> takes one argument
Ast::Root vectorArity() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(Ast::Type{ "vector", { base("int32"), base("int32") }, {} },
        "v") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// array<int32> -> array<T, N> takes two arguments
Ast::Root arrayArity() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(Ast::Type{ "array", { base("int32") }, {} }, "a") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// int32<string> -> basic types take no template argument
Ast::Root basicTemplateArg() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(Ast::Type{ "int32", { base("string") }, {} }, "x") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

// P<int32> where P is a struct -> structs take no template parameters
Ast::Root structTemplateArg() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::StructType aStructType;
        aStructType.name = "P";
        aStructType.fields = { field(base("int32"), "x") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        Ast::StructType aStructType;
        aStructType.name = "A";
        aStructType.fields = { field(Ast::Type{ "P", { base("int32") }, {} }, "p") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    return aRoot;
}

// two interfaces with the same name
Ast::Root dupInterface() {
    using namespace tb;
    auto aRoot = pkg();
    {
        Ast::Interface aInterface;
        aInterface.name = "I";
        aInterface.methods.push_back(method("a", {}));
        aRoot.interfaces.push_back(std::move(aInterface));
    }
    {
        Ast::Interface aInterface;
        aInterface.name = "I";
        aInterface.methods.push_back(method("b", {}));
        aRoot.interfaces.push_back(std::move(aInterface));
    }
    return aRoot;
}

// package segment that is a C++ keyword
Ast::Root packageKeywordSegment() {
    using namespace tb;
    Ast::Root aRoot;
    aRoot.package.name = "p.new";
    Ast::Interface aInterface;
    aInterface.name = "I";
    aInterface.methods.push_back(method("f", {}));
    aRoot.interfaces.push_back(std::move(aInterface));
    return aRoot;
}

// field name that cannot become a C++ identifier
Ast::Root invalidFieldName() {
    using namespace tb;
    auto aRoot = pkg();
    Ast::StructType aStructType;
    aStructType.name = "A";
    aStructType.fields = { field(base("int32"), "x-y") };
    aRoot.structs.push_back(std::move(aStructType));
    return aRoot;
}

void run(const Case& aCase) {
    Sema::Result aSemaResult = Sema::analyze(aCase.make());
    if (aSemaResult.errors.empty()) {
        std::cout << "[FAIL] " << aCase.name << " : expected errors, got none\n";
        ++gFailures;
        return;
    }

    for (const auto& aNeedle : aCase.needles) {
        bool aFound = false;
        for (const auto& aError : aSemaResult.errors)
            if (aError.msg.find(aNeedle) != std::string::npos) { aFound = true; break; }
        if (!aFound) {
            std::cout << "[FAIL] " << aCase.name << " : missing '" << aNeedle << "'. actual:\n";
            for (const auto& aError : aSemaResult.errors)
                std::cout << "    " << aError.line << ":" << aError.col << " "
                          << aError.msg << "\n";
            ++gFailures;
            return;
        }
    }
    std::cout << "[ OK ] " << aCase.name << "\n";
}

} // namespace

int main() {
    const std::vector<Case> aCases = {
        { "struct self by value",   { "is recursive" }, selfDirect },
        { "struct self via vector", { "is recursive" }, selfViaVector },
        { "struct mutual recursion",{ "is recursive" }, selfMutual },
        { "struct declared later",  { "must be declared before" }, forwardStructRef },
        { "alias cycle",            { "alias cycle" }, aliasCycle },
        { "map key is struct",      { "map key must resolve to a basic type" }, mapKeyStruct },
        { "unknown type",           { "unknown type" }, unknownType },
        { "array size 0",           { "must be a positive integer" }, arrayZero },
        { "map arity",              { "map<K, V>: only need 2 template args" }, mapArity },
        { "dup member (method/method)",  { "duplicate member" }, dupMember },
        { "dup member (method/property)",{ "duplicate member" }, dupMemberMethodProp },
        { "dup struct field",       { "duplicate field" }, dupField },
        { "dup type (struct/alias)",{ "duplicate type name" }, dupType },
        { "reserved type name",     { "is reserved" }, reservedName },
        { "empty struct",           { "must have at least one field" }, emptyStruct },
        { "@timeout on property",   { "@timeout is only for method" }, timeoutOnProperty },
        { "unknown annotation",     { "unknown annotation" }, unknownAnnotation },
        { "member is C++ keyword",  { "is a C++ keyword" }, keywordMember },
        { "interface is C++ keyword", { "is a C++ keyword" }, keywordIface },
        { "@readonly on method",    { "@readonly is only for property" }, readonlyOnMethod },
        { "@timeout(0)",            { "@timeout needs a positive integer" }, timeoutZero },
        { "@timeout(abc)",          { "@timeout needs a positive integer" }, timeoutNotANumber },
        { "@readonly with value",   { "@readonly doesn't require a value" }, readonlyWithValue },
        { "@deprecated with value", { "@deprecated doesn't require a value" }, deprecatedWithValue },
        { "@sync on property",      { "@sync is only for method" }, syncOnProperty },
        { "duplicate annotation",   { "duplicate annotation" }, dupAnnotation },
        { "vector<T> arity",        { "vector<T> only need 1 template arg" }, vectorArity },
        { "array<T, N> arity",      { "array<T, N>: only need 2 template args" }, arrayArity },
        { "basic type with template arg", { "cannot have template arg" }, basicTemplateArg },
        { "struct with template arg",     { "does not accept template parameters" }, structTemplateArg },
        { "dup interface name",     { "duplicate interface" }, dupInterface },
        { "package segment is C++ keyword", { "package segment 'new' is a C++ keyword" },
          packageKeywordSegment },
        { "field name is not an identifier", { "is not a valid identifier in struct 'A'" },
          invalidFieldName },
    };

    for (const auto& aCase : aCases) run(aCase);

    if (gFailures != 0) {
        std::cout << "[RESULT] " << gFailures << "/" << aCases.size() << " case(s) FAILED\n";
        return 1;
    }
    std::cout << "[RESULT] all " << aCases.size() << " negative cases rejected correctly\n";
    return 0;
}
