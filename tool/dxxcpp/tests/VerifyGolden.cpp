//! Positive case: hand-built golden AST -> Sema::analyze -> no errors, IR dumped.
//! Standing regression entry for the whole validate + generateIr chain.
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Ast.hpp"
#include "Ir.hpp"
#include "Sema.hpp"
#include "AstBuilder.hpp"


using namespace Ast;

// ---------------- build the sample Root (calculator example of note/dxx.md) ----------------
static Root makeGoldenRoot() {
    using namespace tb;
    Root aRoot;
    aRoot.package.name = "com.example.calc";

    // aliases: UserId->int32, ConfigMap->map<string,string>, UserMap->map<UserId,string> (chained)
    aRoot.alias.push_back(AliasType{ "UserId", base("int32"), {} });
    aRoot.alias.push_back(AliasType{ "ConfigMap", mp(base("string"), base("string")), {} });
    aRoot.alias.push_back(AliasType{ "UserMap", mp(named("UserId"), base("string")), {} });

    // structs
    {
        StructType aStructType;
        aStructType.name = "Point";
        aStructType.fields = { field(base("int32"), "x"), field(base("int32"), "y") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        StructType aStructType;
        aStructType.name = "Rectangle";
        aStructType.fields = { field(named("Point"), "topLeft"),
            field(named("Point"), "bottomRight") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        StructType aStructType;
        aStructType.name = "User";
        aStructType.fields = { field(named("UserId"), "id"), field(base("string"), "name") };
        aRoot.structs.push_back(std::move(aStructType));
    }

    // interface Calculator
    {
        Interface aInterface;
        aInterface.name = "Calculator";
        aInterface.methods.push_back(method("add",
            { field(base("int32"), "a"), field(base("int32"), "b") },
            base("int32")));
        aInterface.methods.push_back(method("notify", { field(base("string"), "msg") }));
        aInterface.methods.push_back(method("translate",
            { field(named("Point"), "p"), field(base("int32"), "dx"),
              field(base("int32"), "dy") },
            named("Point")));
        aInterface.methods.push_back(method("transformPolyline",
            { field(vect(named("Point")), "points"), field(base("int32"), "dx"),
              field(base("int32"), "dy") },
            vect(named("Point"))));
        aInterface.methods.push_back(method("getConfig", {}, named("ConfigMap")));
        aInterface.methods.push_back(method("getUserMap", {}, named("UserMap")));
        aInterface.methods.push_back(method("addUser", { field(named("User"), "u") },
            named("User")));
        aInterface.methods.push_back(method("slowOp", { field(base("int32"), "n") },
            base("int32"), { ann("timeout", "3000") }));
        aInterface.methods.push_back(method("legacy",
            { field(base("int32"), "code"), field(base("string"), "msg") },
            std::nullopt, { ann("deprecated") }));

        aInterface.signals.push_back(signal("valueChanged",
            { field(base("int32"), "oldVal"), field(base("int32"), "newVal") }));
        aInterface.signals.push_back(signal("pointMoved",
            { field(named("Point"), "oldPos"), field(named("Point"), "newPos") }));

        aInterface.properties.push_back(property("version", base("string"), "{\"1.0.0\"}",
            { ann("readonly") }));
        aInterface.properties.push_back(property("counter", base("int32"), "{0}"));
        aInterface.properties.push_back(property("enabled", base("bool"), "{true}"));
        aInterface.properties.push_back(property("metadata",
            mp(base("string"), base("string")), "{}"));

        aRoot.interfaces.push_back(std::move(aInterface));
    }

    // interface Logger
    {
        Interface aInterface;
        aInterface.name = "Logger";
        aInterface.methods.push_back(method("log", { field(base("string"), "msg") },
            base("bool")));
        aInterface.signals.push_back(signal("errorOccurred",
            { field(base("int32"), "code"), field(base("string"), "msg") }));
        aRoot.interfaces.push_back(std::move(aInterface));
    }

    return aRoot;
}

// ---------------- IR dump helpers: basic signatures / struct signatures ----------------
static const std::unordered_map<std::string, std::string>& baseSigTable() {
    static const std::unordered_map<std::string, std::string> aTable = {
        { "int8", "y" }, { "int16", "n" }, { "int32", "i" }, { "int64", "x" },
        { "uint8", "y" }, { "uint16", "q" }, { "uint32", "u" }, { "uint64", "t" },
        { "float", "d" }, { "double", "d" }, { "bool", "b" }, { "string", "s" },
        { "bytes", "ay" } };
    return aTable;
}

static std::string sig(const Ir::Root& aRoot, Ir::TypeId aId) {
    const Ir::TypeNode& aNode = aRoot.type(aId);
    if (const auto* aBaseType = std::get_if<Ir::TypeBase>(&aNode.kind)) {
        const std::string& aName = aRoot.nameOf(aBaseType->name);
        auto aIt = baseSigTable().find(aName);
        return aIt != baseSigTable().end() ? aIt->second : ("?" + aName);
    }
    if (const auto* aVector = std::get_if<Ir::TypeVector>(&aNode.kind))
        return "a" + sig(aRoot, aVector->element);
    if (const auto* aArray = std::get_if<Ir::TypeArray>(&aNode.kind))
        return "a" + sig(aRoot, aArray->element);
    if (const auto* aMapType = std::get_if<Ir::TypeMap>(&aNode.kind))
        return "a{" + sig(aRoot, aMapType->key) + sig(aRoot, aMapType->value) + "}";
    if (const auto* aStructType = std::get_if<Ir::TypeStruct>(&aNode.kind)) {
        std::string aOut = "(";
        for (const auto& aField : aRoot.structBy(aStructType->def).fields)
            aOut += sig(aRoot, aField.type);
        return aOut + ")";
    }
    return "?";
}

static std::string typeName(const Ir::Root& aRoot, Ir::TypeId aId) {
    // leaf name for Base/Struct; containers use a placeholder
    return aRoot.leafName(aRoot.type(aId));
}

// ---------------- output ----------------
static void dumpIr(const Ir::Root& aRoot) {
    std::cout << "== IR dump ==\npackage: " << aRoot.package << "\n";

    std::cout << "-- structs (" << aRoot.structs.size() << ") --\n";
    for (const auto& aStruct : aRoot.structs) {
        std::cout << "  " << aStruct.name << " : ";
        for (const auto& aField : aStruct.fields)
            std::cout << aField.name << ":" << typeName(aRoot, aField.type) << " ";
        std::cout << "\n";
    }

    std::cout << "-- aliases (" << aRoot.aliases.size() << ") --\n";
    for (const auto& aAlias : aRoot.aliases)
        std::cout << "  " << aAlias.name << " -> sig(" << sig(aRoot, aAlias.target) << ")\n";

    std::cout << "-- interfaces (" << aRoot.interfaces.size() << ") --\n";
    for (const auto& aInterface : aRoot.interfaces) {
        std::cout << "  iface " << aInterface.name << "\n";
        for (const auto& aMethod : aInterface.methods) {
            std::string aIn;
            for (const auto& aParam : aMethod.params) aIn += sig(aRoot, aParam.type);
            std::string aOut = aMethod.ret ? sig(aRoot, *aMethod.ret) : "";
            std::cout << "    method " << aMethod.name << " in(" << aIn << ") out(" << aOut << ")"
                      << (aMethod.deprecated ? " deprecated" : "")
                      << (aMethod.timeoutUsec ?
                          " timeout=" + std::to_string(*aMethod.timeoutUsec) : "")
                      << "\n";
        }
        for (const auto& aSignal : aInterface.signals) {
            std::string aIn;
            for (const auto& aParam : aSignal.params) aIn += sig(aRoot, aParam.type);
            std::cout << "    signal " << aSignal.name << " in(" << aIn << ")\n";
        }
        for (const auto& aProperty : aInterface.properties)
            std::cout << "    property " << aProperty.name << " type("
                      << sig(aRoot, aProperty.type) << ")"
                      << " default='" << aProperty.defaultValue << "'"
                      << (aProperty.readonly ? " readonly" : "") << "\n";
    }
    std::cout << "  arena types = " << aRoot.types.size() << ", names = "
              << aRoot.typeNames.size() << "\n";
}

// ---------------- assertions on the IR shape ----------------
//! The dump above is for humans; these checks are what actually guards the IR
int gFail = 0;

void ok(const std::string& aWhat) {
    std::cout << "  [ OK ] " << aWhat << "\n";
}

void fail(const std::string& aWhat, const std::string& aDetail) {
    std::cout << "  [FAIL] " << aWhat << " : " << aDetail << "\n";
    ++gFail;
}

void expectEq(const std::string& aWhat,
  const std::string& aActual, const std::string& aExpected) {
    if (aActual == aExpected) {
        ok(aWhat + " = '" + aActual + "'");
        return;
    }

    fail(aWhat, "expect '" + aExpected + "', got '" + aActual + "'");
}

void expectEq(const std::string& aWhat,
  std::size_t aActual, std::size_t aExpected) {
    if (aActual == aExpected) {
        ok(aWhat + " = " + std::to_string(aActual));
        return;
    }

    fail(aWhat, "expect " + std::to_string(aExpected) + ", got " + std::to_string(aActual));
}

void expectEq(const std::string& aWhat, bool aActual, bool aExpected) {
    expectEq(aWhat, std::string(aActual ? "true" : "false"),
        std::string(aExpected ? "true" : "false"));
}

std::string paramsSig(const Ir::Root& aRoot,
  const std::vector<Ir::Parameter>& aParams) {
    std::string aOut;
    for (const auto& aParam : aParams) {
        aOut += sig(aRoot, aParam.type);
    }

    return aOut;
}

std::string retSig(const Ir::Root& aRoot, const std::optional<Ir::TypeId>& aId) {
    return aId ? sig(aRoot, *aId) : std::string();
}

const Ir::Method* findMethod(const Ir::Interface& aIfce, const std::string& aName) {
    for (const auto& aMethod : aIfce.methods) {
        if (aMethod.name == aName) {
            return &aMethod;
        }
    }

    return nullptr;
}

const Ir::Property* findProperty(const Ir::Interface& aIfce, const std::string& aName) {
    for (const auto& aProperty : aIfce.properties) {
        if (aProperty.name == aName) {
            return &aProperty;
        }
    }

    return nullptr;
}

void expectMethod(const Ir::Root& aRoot, const Ir::Interface& aIfce,
  const std::string& aName, const std::string& aInSig, const std::string& aOutSig) {
    const Ir::Method* aMethod = findMethod(aIfce, aName);
    if (!aMethod) {
        fail("method " + aName, "not found");
        return;
    }

    expectEq("method " + aName + " in", paramsSig(aRoot, aMethod->params), aInSig);
    expectEq("method " + aName + " out", retSig(aRoot, aMethod->ret), aOutSig);
}

void expectProperty(const Ir::Root& aRoot, const Ir::Interface& aIfce,
  const std::string& aName, const std::string& aSig,
  const std::string& aDefault, bool aReadonly) {
    const Ir::Property* aProperty = findProperty(aIfce, aName);
    if (!aProperty) {
        fail("property " + aName, "not found");
        return;
    }

    expectEq("property " + aName + " type", sig(aRoot, aProperty->type), aSig);
    expectEq("property " + aName + " default", aProperty->defaultValue, aDefault);
    expectEq("property " + aName + " readonly", aProperty->readonly, aReadonly);
}

void caseGoldenIr(const Ir::Root& aRoot) {
    std::cout << "\n===== IR shape =====\n";
    expectEq("package", aRoot.package, "com.example.calc");

    //! structs keep declaration order; a struct signature is its field signature
    expectEq("struct count", aRoot.structs.size(), 3);
    if (aRoot.structs.size() == 3) {
        expectEq("struct[0]", aRoot.structs[0].name, "Point");
        expectEq("struct[1]", aRoot.structs[1].name, "Rectangle");
        expectEq("struct[2]", aRoot.structs[2].name, "User");
    }

    //! aliases are resolved to their target type
    expectEq("alias count", aRoot.aliases.size(), 3);
    if (aRoot.aliases.size() == 3) {
        expectEq("alias UserId", sig(aRoot, aRoot.aliases[0].target), "i");
        expectEq("alias ConfigMap", sig(aRoot, aRoot.aliases[1].target), "a{ss}");
        expectEq("alias UserMap", sig(aRoot, aRoot.aliases[2].target), "a{is}");
    }

    expectEq("interface count", aRoot.interfaces.size(), 2);
    if (aRoot.interfaces.size() != 2) {
        return;
    }

    //! Calculator: methods / signals / properties
    const Ir::Interface& aCalc = aRoot.interfaces[0];
    expectEq("iface[0]", aCalc.name, "Calculator");
    expectEq("Calculator.methods", aCalc.methods.size(), 9);
    expectEq("Calculator.signals", aCalc.signals.size(), 2);
    expectEq("Calculator.properties", aCalc.properties.size(), 4);

    expectMethod(aRoot, aCalc, "add", "ii", "i");
    expectMethod(aRoot, aCalc, "notify", "s", "");
    expectMethod(aRoot, aCalc, "translate", "(ii)ii", "(ii)");
    expectMethod(aRoot, aCalc, "transformPolyline", "a(ii)ii", "a(ii)");
    expectMethod(aRoot, aCalc, "getConfig", "", "a{ss}");
    expectMethod(aRoot, aCalc, "getUserMap", "", "a{is}");
    expectMethod(aRoot, aCalc, "addUser", "(is)", "(is)");
    expectMethod(aRoot, aCalc, "slowOp", "i", "i");
    expectMethod(aRoot, aCalc, "legacy", "is", "");

    //! method flags: @timeout -> microseconds, @deprecated -> flag
    const Ir::Method* aSlowOp = findMethod(aCalc, "slowOp");
    expectEq("slowOp.timeoutUsec", aSlowOp && aSlowOp->timeoutUsec ? *aSlowOp->timeoutUsec : 0,
        3000000);
    const Ir::Method* aLegacy = findMethod(aCalc, "legacy");
    expectEq("legacy.deprecated", aLegacy != nullptr && aLegacy->deprecated, true);
    const Ir::Method* aAdd = findMethod(aCalc, "add");
    expectEq("add.deprecated", aAdd != nullptr && aAdd->deprecated, false);

    //! signals
    expectEq("signal valueChanged", paramsSig(aRoot, aCalc.signals[0].params), "ii");
    expectEq("signal pointMoved", paramsSig(aRoot, aCalc.signals[1].params), "(ii)(ii)");

    //! properties: type / raw default text / readonly
    expectProperty(aRoot, aCalc, "version", "s", "{\"1.0.0\"}", true);
    expectProperty(aRoot, aCalc, "counter", "i", "{0}", false);
    expectProperty(aRoot, aCalc, "enabled", "b", "{true}", false);
    expectProperty(aRoot, aCalc, "metadata", "a{ss}", "{}", false);

    //! Logger
    const Ir::Interface& aLogger = aRoot.interfaces[1];
    expectEq("iface[1]", aLogger.name, "Logger");
    expectMethod(aRoot, aLogger, "log", "s", "b");
    expectEq("Logger.signals", aLogger.signals.size(), 1);
    expectEq("signal errorOccurred", paramsSig(aRoot, aLogger.signals[0].params), "is");
}

int main() {
    Root aRoot = makeGoldenRoot();
    Sema::Result aSemaResult = Sema::analyze(aRoot);

    if (aSemaResult.errors.empty()) {
        std::cout << "[OK] validate + generateIr passed\n";
        if (aSemaResult.ir) {
            dumpIr(*aSemaResult.ir);
            caseGoldenIr(*aSemaResult.ir);
            if (gFail == 0) {
                std::cout << "\n[RESULT] all golden IR checks passed\n";
                return 0;
            }

            std::cout << "\n[RESULT] " << gFail << " golden check(s) FAILED\n";
            return 1;
        }

        std::cout << "[FAIL] no IR returned (but no errors)\n";
        return 1;
    }

    std::cout << "[FAIL] " << aSemaResult.errors.size() << " error(s):\n";
    for (const auto& aError : aSemaResult.errors)
        std::cout << "  " << aError.line << ":" << aError.col << " " << aError.msg << "\n";
    return 1;
}
