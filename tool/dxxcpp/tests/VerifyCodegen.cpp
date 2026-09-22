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
        //! void + @timeout: the call keeps the explicit <void, TimeoutUsec>
        aInterface.methods.push_back(method("ping", {}, std::nullopt,
            { ann("timeout", "500") }));
        aInterface.methods.push_back(method("notify", { field(base("string"), "msg") }));
        aInterface.methods.push_back(method("legacy", { field(base("int32"), "code") },
            std::nullopt, { ann("deprecated") }));
        aInterface.methods.push_back(method("syncOnly", { field(base("int32"), "val") },
            base("int32"), { ann("sync") }));
        aInterface.methods.push_back(method("asyncOnly", { field(base("int32"), "val") },
            base("bool"), { ann("async") }));

        aInterface.signals.push_back(signal("valueChanged",
            { field(base("int32"), "old"), field(base("int32"), "new") }));
        aInterface.signals.push_back(signal("legacyEvent",
            { field(base("int32"), "code") }, { ann("deprecated") }));

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
    const std::string aSkeletonHeader = Codegen::genSkeletonHeader(aIr, aInterface);
    const std::string aSkeletonSrc = Codegen::genSkeletonSource(aIr, aInterface);
    const std::string aProxyHeader = Codegen::genProxyHeader(aIr, aInterface);
    const std::string aProxySrc = Codegen::genProxySource(aIr, aInterface);

    section("Types");
    std::cout << aTypes << "\n";
    section("Skeleton");
    std::cout << aSkeletonHeader << "\n";
    section("Skeleton.cpp");
    std::cout << aSkeletonSrc << "\n";
    section("Proxy");
    std::cout << aProxyHeader << "\n";
    section("Proxy.cpp");
    std::cout << aProxySrc << "\n";

    // ---- Types ----
    expectContains("types: ns", aTypes, "namespace Com::Example::Calc");
    //! Bind the field to its struct: a bare "std::int32_t x;" would also pass
    //! if the field moved into another struct
    expectContains("types: struct + first field", aTypes,
        "struct Point {\n    std::int32_t x;");
    expectContains("types: using", aTypes, "using ConfigMap = std::map<std::string, std::string>;");
    expectContains("types: static_assert", aTypes, "static_assert(std::is_aggregate_v<Point>");
    // ---- Skeleton ----
    expectContains("skel: class", aSkeletonHeader,
        "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>");
    expectContains("skel: path", aSkeletonHeader, "DBUSXX_PATH(\"/com/example/calc\")");
    expectContains("skel: iface", aSkeletonHeader, "DBUSXX_IFACE(\"com.example.calc.Calculator\")");
    expectContains("skel: method", aSkeletonHeader, "DBUSXX_METHOD(add)");
    expectContains("skel: void", aSkeletonHeader, "DBUSXX_METHOD(notify)");
    expectContains("skel: signal", aSkeletonHeader,
                   "DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)");
    expectContains("skel: prop RO", aSkeletonHeader,
        "DBUSXX_PROPERTY_RO(version, std::string, {\"1.0.0\"})");
    expectContains("skel: prop RW", aSkeletonHeader,
        "DBUSXX_PROPERTY_RW(counter, std::int32_t, {0})");
    expectContains("skel: prop multi value", aSkeletonHeader,
        "DBUSXX_PROPERTY_RW(samples, std::vector<std::int32_t>, {1, 2, 3})");
    expectContains("skel: prop map mask", aSkeletonHeader,
        "DBUSXX_PROPERTY_RW(metadata, decltype(std::map<std::string, std::string>{}), {})");
    expectContains("skel: prop no init (scalar)", aSkeletonHeader,
        "DBUSXX_PROPERTY_RW(plain, std::int32_t, std::int32_t{})");
    expectContains("skel: prop no init (map)", aSkeletonHeader,
        "DBUSXX_PROPERTY_RW(config, decltype(std::map<std::string, std::string>{}), {})");
    //! Each comment must sit on the member it marks, not float around
    expectContains("skel: deprecated method is comment only", aSkeletonHeader,
        "//! @deprecated\n    void legacy(std::int32_t code);");
    expectContains("skel: deprecated signal is comment only", aSkeletonHeader,
        "//! @deprecated\n    DBUSXX_SIGNAL(legacyEvent, std::int32_t)");
    expectNotContains("skel: no [[deprecated]]", aSkeletonHeader, "[[deprecated]]");
    //! The service name moved into the generated source
    expectNotContains("skel: header carries no service name", aSkeletonHeader,
        "DBUSXX_SERVICE_NAME");

    // ---- Skeleton.cpp: the out-of-line Server definitions ----
    expectContains("skel src: header include", aSkeletonSrc,
        "#include \"CalculatorSkeleton.hpp\"");
    expectContains("skel src: service name guard", aSkeletonSrc,
        "#ifndef DBUSXX_SERVICE_NAME\n#error");
    expectContains("skel src: ctor takes the interface", aSkeletonSrc,
        "CalculatorServer::CalculatorServer(std::unique_ptr<CalculatorInterface> aIface)");
    expectContains("skel src: ctor forwards the service name", aSkeletonSrc,
        ": Dbusxx::Server<CalculatorServer>(DBUSXX_SERVICE_NAME)");
    expectContains("skel src: method body (with return)", aSkeletonSrc,
        "std::int32_t CalculatorServer::add(std::int32_t a, std::int32_t b) {\n"
        "    return mIface->add(a, b);\n}");
    expectContains("skel src: method body (void)", aSkeletonSrc,
        "void CalculatorServer::notify(const std::string& msg) {\n"
        "    mIface->notify(msg);\n}");
    //! Registration stays in the header (DBUSXX_METHOD refers to &Self::method)
    expectNotContains("skel src: no registration macros", aSkeletonSrc, "DBUSXX_METHOD");
    expectNotContains("skel src: no registration macros (signal)", aSkeletonSrc, "DBUSXX_SIGNAL");

    // ---- Proxy ----
    expectContains("proxy: service name guard", aProxyHeader,
        "#ifndef DBUSXX_SERVICE_NAME\n#error");
    expectContains("proxy: ctor takes no service name", aProxyHeader,
        "explicit CalculatorProxy();");
    expectContains("proxy: declaration", aProxyHeader,
        "[[nodiscard]] Dbusxx::Reply<std::int32_t> add(std::int32_t a, std::int32_t b);");
    expectContains("proxy: sync call", aProxySrc, "mClient.callSync<std::int32_t>(\"add\", a, b)");
    expectContains("proxy: struct call", aProxySrc, "callSync<Point>(\"translate\"");
    expectContains("proxy: timeout", aProxySrc,
        "callSync<std::map<std::string, std::string>, 3000000>(\"getConfig\"");
    expectContains("proxy: oneway", aProxySrc, "callSync(\"notify\"");
    //! Bind the attribute to the declaration it marks: a bare "[[deprecated]]"
    //! would also pass if it drifted onto a member that is not deprecated
    expectContains("proxy: deprecated method", aProxyHeader,
        "[[deprecated]]\n    [[nodiscard]] Dbusxx::Reply<void> legacy(std::int32_t code);");
    expectContains("proxy src: header include", aProxySrc,
        "#include \"CalculatorProxy.hpp\"");
    //! The well-known name is compiled in, like on the server side
    expectContains("proxy src: ctor compiles the name in", aProxySrc,
        "CalculatorProxy::CalculatorProxy()\n"
        "    : mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,");

    // ---- Proxy: signal listeners ----
    expectContains("proxy: signal listener", aProxyHeader,
        "[[nodiscard]] Dbusxx::Status onValueChanged("
        "std::function<void(std::int32_t old, std::int32_t new_)> aCallback);");
    expectContains("proxy: signal listener call", aProxySrc,
        "mClient.listenSignal(\"valueChanged\", std::move(aCallback))");
    expectContains("proxy: deprecated signal listener", aProxyHeader,
        "[[deprecated]]\n    [[nodiscard]] Dbusxx::Status onLegacyEvent(");

    // ---- Proxy: a void method keeps the explicit <void, TimeoutUsec> ----
    expectContains("proxy: void + timeout", aProxySrc,
        "mClient.callSync<void, 500000>(\"ping\")");
    expectContains("proxy: void + timeout async", aProxySrc,
        "mClient.callAsync<void, 500000>(\"ping\")");
    expectContains("proxy: void + timeout callback", aProxySrc,
        "mClient.callAsync<void, 500000>(\"ping\", std::move(aCallback))");

    // ---- Proxy: a method without @sync/@async gets both shapes ----
    expectContains("proxy: async handle", aProxyHeader,
        "Dbusxx::PendingReply<std::int32_t> addAsync(");
    expectContains("proxy: async callback", aProxyHeader,
        "Dbusxx::Status addAsync(std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback,");
    expectContains("proxy: async call keeps timeout", aProxySrc,
        "mClient.callAsync<std::map<std::string, std::string>, 3000000>(\"getConfig\", "
        "std::move(aCallback))");

    // ---- Proxy: @sync -> the synchronous shape only ----
    expectContains("proxy: @sync signature", aProxyHeader,
        "[[nodiscard]] Dbusxx::Reply<std::int32_t> syncOnly(std::int32_t val)");
    expectContains("proxy: @sync call", aProxySrc,
        "mClient.callSync<std::int32_t>(\"syncOnly\", val)");
    //! The absence checks must name the shape, not the bare "<name>Async":
    //! "asyncOnly" contains "syncOnly", so "syncOnlyAsync" also occurs inside
    //! the generated "asyncOnlyAsync"
    expectNotContains("proxy: @sync has no async handle", aProxyHeader,
        "Dbusxx::PendingReply<std::int32_t> syncOnly");
    expectNotContains("proxy: @sync has no async callback", aProxyHeader,
        "Dbusxx::Status syncOnly(");

    // ---- Proxy: @async -> the two asynchronous shapes only ----
    expectContains("proxy: @async handle", aProxyHeader,
        "[[nodiscard]] Dbusxx::PendingReply<bool> asyncOnlyAsync(std::int32_t val)");
    expectContains("proxy: @async handle call", aProxySrc,
        "mClient.callAsync<bool>(\"asyncOnly\", val)");
    expectContains("proxy: @async callback", aProxyHeader,
        "Dbusxx::Status asyncOnlyAsync(std::function<void(Dbusxx::Reply<bool>)> aCallback, "
        "std::int32_t val)");
    expectContains("proxy: @async callback call", aProxySrc,
        "mClient.callAsync<bool>(\"asyncOnly\", std::move(aCallback), val)");
    expectNotContains("proxy: @async has no sync shape", aProxyHeader,
        "Dbusxx::Reply<bool> asyncOnly(");
    expectNotContains("proxy: @async has no sync call", aProxySrc,
        "callSync<bool>(\"asyncOnly\"");
    if (gFail != 0) {
        std::cout << "[RESULT] " << gFail << " codegen check(s) FAILED\n";
        return 1;
    }
    std::cout << "\n[RESULT] all codegen shape checks passed\n";
    return 0;
}
