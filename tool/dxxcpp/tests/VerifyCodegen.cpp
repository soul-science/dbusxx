//! Codegen smoke test: hand-built AST -> analyze -> the three generated texts,
//! asserting key shapes. Only text shape is checked, not compilation.
#include "Ast.hpp"
#include "Sema.hpp"
#include "Codegen.hpp"
#include "AstBuilder.hpp"

#include <iostream>
#include <string>
#include <vector>


using namespace Ast;

static Root makeCodegenRoot() {
    using namespace tb;
    Root aRoot;
    aRoot.package.name = "com.example.calc";

    aRoot.alias.push_back(AliasType{ "ConfigMap", mp(base("string"), base("string")), {} });

    {
        StructType aStructType;
        aStructType.name = "Point";
        aStructType.fields = { field(base("int32"), "x"), field(base("int32"), "y") };
        aRoot.structs.push_back(std::move(aStructType));
    }
    {
        StructType aStructType;
        aStructType.name = "User";
        aStructType.fields = { field(base("string"), "name") };
        aRoot.structs.push_back(std::move(aStructType));
    }

    {
        Interface aInterface;
        aInterface.name = "Calculator";
        aInterface.methods.push_back(method("add",
            { field(base("int32"), "a"), field(base("int32"), "b") }, base("int32")));
        aInterface.methods.push_back(method("translate",
            { field(named("Point"), "p"), field(base("int32"), "dx") }, named("Point")));
        aInterface.methods.push_back(method("getConfig", {}, named("ConfigMap"),
            { ann("timeout", "3000") }));
        aInterface.methods.push_back(method("notify", { field(base("string"), "msg") }));
        aInterface.methods.push_back(method("legacy", { field(base("int32"), "code") },
            std::nullopt, { ann("deprecated") }));
        aInterface.methods.push_back(method("syncOnly", { field(base("int32"), "val") },
            base("int32"), { ann("sync") }));
        aInterface.methods.push_back(method("asyncOnly", { field(base("int32"), "val") },
            base("bool"), { ann("async") }));

        aInterface.signals.push_back(signal("valueChanged",
            { field(base("int32"), "old"), field(base("int32"), "new") }));

        aInterface.properties.push_back(property("version", base("string"), "{\"1.0.0\"}",
            { ann("readonly") }));
        aInterface.properties.push_back(property("counter", base("int32"), "{0}"));
        aInterface.properties.push_back(property("metadata",
            mp(base("string"), base("string")), "{}"));
        aInterface.properties.push_back(property("samples", vect(base("int32")), "{1, 2, 3}"));
        aInterface.properties.push_back(property("plain", base("int32"), ""));
        aInterface.properties.push_back(property("config",
            mp(base("string"), base("string")), ""));

        aRoot.interfaces.push_back(std::move(aInterface));
    }
    return aRoot;
}

namespace {
int gFail = 0;

void expectContains(const char* aWhat, const std::string& aHay, const std::string& aNeedle) {
    if (aHay.find(aNeedle) != std::string::npos) {
        std::cout << "[ OK ] " << aWhat << ": found \"" << aNeedle << "\"\n";
    } else {
        std::cout << "[FAIL] " << aWhat << ": missing \"" << aNeedle << "\"\n";
        ++gFail;
    }
}

void section(const char* aTitle) {
    std::cout << "\n===== " << aTitle << " =====\n";
}

void expectNotContains(const char* aWhat, const std::string& aHay, const std::string& aNeedle) {
    if (aHay.find(aNeedle) == std::string::npos) {
        std::cout << "[ OK ] " << aWhat << ": absent \"" << aNeedle << "\"\n";
    } else {
        std::cout << "[FAIL] " << aWhat << ": unexpectedly found \"" << aNeedle << "\"\n";
        ++gFail;
    }
}
} // namespace

int main() {
    Root aRoot = makeCodegenRoot();
    Sema::Result aSemaResult = Sema::analyze(aRoot);
    if (!aSemaResult.errors.empty()) {
        std::cout << "[FAIL] sema errors:\n";
        for (const auto& aError : aSemaResult.errors)
            std::cout << "  " << aError.msg << "\n";
        return 1;
    }
    const Ir::Root& aIr = *aSemaResult.ir;
    const Ir::Interface& aInterface = aIr.interfaces[0];

    const std::string aTypes = Codegen::genTypesHeader(aIr);
    const std::string aSkeleton = Codegen::genSkeletonHeader(aIr, aInterface);
    const std::string aProxy = Codegen::genProxyHeader(aIr, aInterface);

    section("Types");
    std::cout << aTypes << "\n";
    section("Skeleton");
    std::cout << aSkeleton << "\n";
    section("Proxy");
    std::cout << aProxy << "\n";

    // ---- Types ----
    expectContains("types: ns", aTypes, "namespace Com::Example::Calc");
    expectContains("types: struct", aTypes, "struct Point {");
    expectContains("types: field", aTypes, "std::int32_t x;");
    expectContains("types: using", aTypes, "using ConfigMap = std::map<std::string, std::string>;");
    expectContains("types: static_assert", aTypes, "static_assert(std::is_aggregate_v<Point>");
    // ---- Skeleton ----
    expectContains("skel: class", aSkeleton, "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>");
    expectContains("skel: path", aSkeleton, "DBUSXX_PATH(\"/com/example/calc\")");
    expectContains("skel: iface", aSkeleton, "DBUSXX_IFACE(\"com.example.calc.Calculator\")");
    expectContains("skel: method", aSkeleton, "DBUSXX_METHOD(add)");
    expectContains("skel: void", aSkeleton, "DBUSXX_METHOD(notify)");
    expectContains("skel: signal", aSkeleton,
                   "DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)");
    expectContains("skel: prop RO", aSkeleton, "DBUSXX_PROPERTY_RO(version, std::string, {\"1.0.0\"})");
    expectContains("skel: prop RW", aSkeleton, "DBUSXX_PROPERTY_RW(counter, std::int32_t, {0})");
    expectContains("skel: prop multi value", aSkeleton,
        "DBUSXX_PROPERTY_RW(samples, std::vector<std::int32_t>, {1, 2, 3})");
    expectContains("skel: prop map mask", aSkeleton,
        "DBUSXX_PROPERTY_RW(metadata, decltype(std::map<std::string, std::string>{}), {})");
    expectContains("skel: prop no init (scalar)", aSkeleton,
        "DBUSXX_PROPERTY_RW(plain, std::int32_t, std::int32_t{})");
    expectContains("skel: prop no init (map)", aSkeleton,
        "DBUSXX_PROPERTY_RW(config, decltype(std::map<std::string, std::string>{}), {})");
    expectContains("skel: deprecated is comment only", aSkeleton, "// @deprecated");
    expectNotContains("skel: no [[deprecated]]", aSkeleton, "[[deprecated]]");

    // ---- Proxy ----
    expectContains("proxy: sync", aProxy, "callSync<std::int32_t>(\"add\"");
    expectContains("proxy: struct", aProxy, "callSync<Point>(\"translate\"");
    expectContains("proxy: timeout", aProxy, "callSync<std::map<std::string, std::string>, 3000000>(\"getConfig\"");
    expectContains("proxy: oneway", aProxy, "callSync(\"notify\"");
    expectContains("proxy: deprecated", aProxy, "[[deprecated]]");

    // ---- Proxy: a method without @sync/@async gets both shapes ----
    expectContains("proxy: async handle", aProxy,
        "Dbusxx::PendingReply<std::int32_t> addAsync(");
    expectContains("proxy: async callback", aProxy,
        "Dbusxx::Status addAsync(std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback,");
    expectContains("proxy: async call keeps timeout", aProxy,
        "mClient.callAsync<std::map<std::string, std::string>, 3000000>(\"getConfig\", "
        "std::move(aCallback))");

    // ---- Proxy: @sync -> the synchronous shape only ----
    expectContains("proxy: @sync signature", aProxy,
        "[[nodiscard]] Dbusxx::Reply<std::int32_t> syncOnly(std::int32_t val)");
    expectContains("proxy: @sync call", aProxy,
        "mClient.callSync<std::int32_t>(\"syncOnly\", val)");
    //! The absence checks must name the shape, not the bare "<name>Async":
    //! "asyncOnly" contains "syncOnly", so "syncOnlyAsync" also occurs inside
    //! the generated "asyncOnlyAsync"
    expectNotContains("proxy: @sync has no async handle", aProxy,
        "Dbusxx::PendingReply<std::int32_t> syncOnly");
    expectNotContains("proxy: @sync has no async callback", aProxy,
        "Dbusxx::Status syncOnly(");

    // ---- Proxy: @async -> the two asynchronous shapes only ----
    expectContains("proxy: @async handle", aProxy,
        "[[nodiscard]] Dbusxx::PendingReply<bool> asyncOnlyAsync(std::int32_t val)");
    expectContains("proxy: @async handle call", aProxy,
        "mClient.callAsync<bool>(\"asyncOnly\", val)");
    expectContains("proxy: @async callback", aProxy,
        "Dbusxx::Status asyncOnlyAsync(std::function<void(Dbusxx::Reply<bool>)> aCallback, "
        "std::int32_t val)");
    expectContains("proxy: @async callback call", aProxy,
        "mClient.callAsync<bool>(\"asyncOnly\", std::move(aCallback), val)");
    expectNotContains("proxy: @async has no sync shape", aProxy, "Dbusxx::Reply<bool> asyncOnly(");
    expectNotContains("proxy: @async has no sync call", aProxy,
        "callSync<bool>(\"asyncOnly\"");
    if (gFail != 0) {
        std::cout << "[RESULT] " << gFail << " codegen check(s) FAILED\n";
        return 1;
    }
    std::cout << "\n[RESULT] all codegen shape checks passed\n";
    return 0;
}
