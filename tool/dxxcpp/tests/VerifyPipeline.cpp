//! End-to-end regression: .dxx text -> Lexer -> Parser -> Sema -> Codegen
//! usage: verify_pipeline <Sample.dxx>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Codegen.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Sema.hpp"


namespace {
int gFail = 0;

void ok(const std::string& aWhat) {
    std::cout << "  [ OK ] " << aWhat << "\n";
}

void fail(const std::string& aWhat, const std::string& aDetail) {
    std::cout << "  [FAIL] " << aWhat << " : " << aDetail << "\n";
    ++gFail;
}

void section(const std::string& aTitle) {
    std::cout << "\n===== " << aTitle << " =====\n";
}

bool expectContains(const std::string& aWhat,
  const std::string& aHay, const std::string& aNeedle) {
    if (aHay.find(aNeedle) != std::string::npos) {
        ok(aWhat + ": found \"" + aNeedle + "\"");
        return true;
    }

    fail(aWhat, "missing \"" + aNeedle + "\"");
    return false;
}

bool readFile(const std::string& aPath, std::string* aOut) {
    std::ifstream aStream(aPath);
    if (!aStream) {
        return false;
    }

    std::ostringstream aBuffer;
    aBuffer << aStream.rdbuf();
    *aOut = aBuffer.str();
    return true;
}

//! Keyword parameter names (new/class) must become new_ in the generated code
void caseKeywordParamName() {
    section("keyword parameter name is sanitized");
    const std::string aSrc =
        "package com.example.k;\n"
        "interface I {\n"
        "    method f(int32 new, string class) -> bool;\n"
        "};\n";

    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Sema::Result aSemaResult = Sema::analyze(*aParserResult.root);
    if (!aSemaResult.ir) {
        fail("sema", aSemaResult.errors.empty() ? "no ir" : aSemaResult.errors[0].msg);
        return;
    }

    const Ir::Interface& aInterface = aSemaResult.ir->interfaces[0];
    const std::string aSkeleton = Codegen::genSkeletonHeader(*aSemaResult.ir, aInterface);
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir, aInterface);
    const std::string aProxySrc = Codegen::genProxySource(*aSemaResult.ir, aInterface);
    expectContains("skeleton: new_", aSkeleton, "std::int32_t new_");
    expectContains("skeleton: class_", aSkeleton, "const std::string& class_");
    expectContains("proxy src: callSync args", aProxySrc, "callSync<bool>(\"f\", new_, class_)");
}

//! Only methods live in the Proxy, so <method>Async may also be a property name
void caseAsyncSuffixOnProperty() {
    section("a property named <method>Async is not a collision");
    const std::string aSrc =
        "package com.example.a;\n"
        "interface I {\n"
        "    method f(int32 val) -> int32;\n"
        "    property fAsync -> int32{0};\n"
        "};\n";

    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Sema::Result aSemaResult = Sema::analyze(*aParserResult.root);
    if (!aSemaResult.ir) {
        fail("sema", aSemaResult.errors.empty() ? "no ir" : aSemaResult.errors[0].msg);
        return;
    }

    ok("method f + property fAsync accepted");
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("proxy: async shape kept", aProxy,
        "Dbusxx::PendingReply<std::int32_t> fAsync(std::int32_t val)");
}

//! @sync generates no callback overload, so "aCallback" is a legal parameter name
void caseSyncOnlyCallbackParam() {
    section("@sync: a parameter named like the generated callback is fine");
    const std::string aSrc =
        "package com.example.a;\n"
        "interface I {\n"
        "    @sync method f(int32 aCallback) -> int32;\n"
        "};\n";

    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Sema::Result aSemaResult = Sema::analyze(*aParserResult.root);
    if (!aSemaResult.ir) {
        fail("sema", aSemaResult.errors.empty() ? "no ir" : aSemaResult.errors[0].msg);
        return;
    }

    ok("@sync method with a parameter named aCallback accepted");
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    const std::string aProxySrc = Codegen::genProxySource(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("proxy src: only the sync shape", aProxySrc,
        "mClient.callSync<std::int32_t>(\"f\", aCallback)");
}

//! Names only collide when both are really generated: @async drops the bare name
void caseAsyncOnlyNamesAreFree() {
    section("@async: the bare name stays free for other shapes");
    const std::string aSrc =
        "package com.example.a;\n"
        "interface I {\n"
        "    method f(int32 val) -> int32;\n"
        "    @async method fAsync(int32 val) -> bool;\n"
        "    signal valueChanged(int32 oldVal);\n"
        "    @async method onValueChanged(int32 val) -> int32;\n"
        "};\n";

    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Sema::Result aSemaResult = Sema::analyze(*aParserResult.root);
    if (!aSemaResult.ir) {
        fail("sema", aSemaResult.errors.empty() ? "no ir" : aSemaResult.errors[0].msg);
        return;
    }

    ok("@async methods do not occupy the bare name");
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("proxy: async shape of the plain method", aProxy,
        "Dbusxx::PendingReply<std::int32_t> fAsync(std::int32_t val)");
    expectContains("proxy: signal listener coexists", aProxy,
        "Dbusxx::Status onValueChanged(std::function<void(std::int32_t oldVal)> aCallback)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: verify_pipeline <Sample.dxx>\n";
        return 1;
    }

    std::string aSrc;
    if (!readFile(argv[1], &aSrc)) {
        std::cout << "[FAIL] cannot read " << argv[1] << "\n";
        return 1;
    }

    section("lexer");
    const Lexer::Result aLexResult = Lexer::tokenize(aSrc);
    if (!aLexResult.errors.empty()) {
        fail("no lex error", "'" + aLexResult.errors[0].msg + "' at " +
            std::to_string(aLexResult.errors[0].line) + ":" +
            std::to_string(aLexResult.errors[0].col));
        return 1;
    }

    std::cout << "  [ OK ] " << aLexResult.tokens.size() << " token(s), 0 error\n";

    section("parser");
    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.errors.empty()) {
        fail("no parse error", "'" + aParserResult.errors[0].msg + "' at " +
            std::to_string(aParserResult.errors[0].line) + ":" +
            std::to_string(aParserResult.errors[0].col));
        return 1;
    }

    if (!aParserResult.root) {
        fail("root", "nullopt without errors");
        return 1;
    }

    const Ast::Root& aRoot = *aParserResult.root;
    std::cout << "  [ OK ] package=" << aRoot.package.name
              << " structs=" << aRoot.structs.size()
              << " aliases=" << aRoot.alias.size()
              << " interfaces=" << aRoot.interfaces.size() << "\n";

    if (aRoot.interfaces.size() != 2) {
        fail("interface count", "expect 2, got " + std::to_string(aRoot.interfaces.size()));
        return 1;
    }

    section("sema");
    const Sema::Result aSemaResult = Sema::analyze(aRoot);
    if (!aSemaResult.errors.empty()) {
        fail("no sema error", "'" + aSemaResult.errors[0].msg + "' at " +
            std::to_string(aSemaResult.errors[0].line) + ":" +
            std::to_string(aSemaResult.errors[0].col));
        return 1;
    }

    if (!aSemaResult.ir) {
        fail("ir", "nullopt without errors");
        return 1;
    }

    std::cout << "  [ OK ] ir built, arena types=" << aSemaResult.ir->types.size()
              << " structs=" << aSemaResult.ir->structs.size() << "\n";

    section("codegen: package-prefixed types header");
    const std::string aTypes = Codegen::genTypesHeader(*aSemaResult.ir);
    const std::string aTypesName = Codegen::typesHeaderName(*aSemaResult.ir);
    if (aTypesName == "ComExampleCalcTypes.hpp") {
        ok("types header name: " + aTypesName);
    } else {
        fail("types header name", "expect ComExampleCalcTypes.hpp, got " + aTypesName);
    }

    expectContains("types: ns", aTypes, "namespace Com::Example::Calc");
    //! Bind the field to its struct: a bare "std::int32_t x;" would also pass
    //! if the field moved into another struct
    expectContains("types: struct + first field", aTypes,
        "struct Point {\n    std::int32_t x;");
    expectContains("types: using", aTypes, "using ConfigMap = std::map<std::string, std::string>;");
    expectContains("types: static_assert", aTypes, "static_assert(std::is_aggregate_v<Point>");
    expectContains("types: operator== per field", aTypes,
        "bool operator==(const Point& aOther) const {\n"
        "        return x == aOther.x\n"
        "            && y == aOther.y;");
    expectContains("types: guard", aTypes, "#ifndef COM_EXAMPLE_CALC_TYPES_HPP");

    section("codegen: CalculatorSkeleton.hpp");
    const std::string aSkeleton = Codegen::genSkeletonHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("skel: class", aSkeleton,
        "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>");
    expectContains("skel: path", aSkeleton, "DBUSXX_PATH(\"/com/example/calc\")");
    expectContains("skel: iface", aSkeleton, "DBUSXX_IFACE(\"com.example.calc.Calculator\")");
    expectContains("skel: types include", aSkeleton,
        "#include \"ComExampleCalcTypes.hpp\"");
    expectContains("skel: method", aSkeleton, "DBUSXX_METHOD(add)");
    expectContains("skel: void method with a string arg", aSkeleton, "DBUSXX_METHOD(notify)");
    expectContains("skel: struct param", aSkeleton,
        "Point translate(const Point& p, std::int32_t dx);");
    expectContains("skel: signal", aSkeleton,
        "DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)");
    expectContains("skel: deprecated signal is a comment", aSkeleton,
        "//! @deprecated\n    DBUSXX_SIGNAL(legacyEvent, std::int32_t)");
    expectContains("skel: prop RO", aSkeleton,
        "DBUSXX_PROPERTY_RO(version, std::string, {\"1.0.0\"})");
    expectContains("skel: prop RW", aSkeleton, "DBUSXX_PROPERTY_RW(counter, std::int32_t, {0})");
    expectContains("skel: prop map mask", aSkeleton,
        "DBUSXX_PROPERTY_RW(metadata, decltype(std::map<std::string, std::string>{}), {})");
    expectContains("skel: prop no init (scalar)", aSkeleton,
        "DBUSXX_PROPERTY_RW(untouched, std::int32_t, std::int32_t{})");
    expectContains("skel: prop no init (map)", aSkeleton,
        "DBUSXX_PROPERTY_RW(tags, decltype(std::map<std::string, std::string>{}), {})");
    expectContains("skel: prop struct init", aSkeleton,
        "DBUSXX_PROPERTY_RW(origin, Point, {1, 2})");
    expectContains("skel: prop nested init", aSkeleton,
        "DBUSXX_PROPERTY_RW(history, std::vector<Point>, {{1, 2}, {3, 4}})");
    expectContains("skel: deprecated method comment", aSkeleton,
        "//! @deprecated\n    void legacy(std::int32_t code);");

    section("codegen: CalculatorSkeleton.cpp");
    const std::string aSkeletonSrc = Codegen::genSkeletonSource(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("skel src: header include", aSkeletonSrc,
        "#include \"CalculatorSkeleton.hpp\"");
    expectContains("skel src: service name guard", aSkeletonSrc,
        "#ifndef DBUSXX_SERVICE_NAME\n#error");
    expectContains("skel src: ctor", aSkeletonSrc,
        "CalculatorServer::CalculatorServer(std::unique_ptr<CalculatorInterface> aIface)");
    expectContains("skel src: method body (with return)", aSkeletonSrc,
        "std::int32_t CalculatorServer::add(std::int32_t a, std::int32_t b) {\n"
        "    return mIface->add(a, b);\n}");
    expectContains("skel src: method body (void)", aSkeletonSrc,
        "void CalculatorServer::notify(const std::string& msg) {\n"
        "    mIface->notify(msg);\n}");

    section("codegen: CalculatorProxy.hpp / .cpp");
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    const std::string aProxySrc = Codegen::genProxySource(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("proxy: sync call", aProxySrc, "mClient.callSync<std::int32_t>(\"add\", a, b)");
    expectContains("proxy: struct call", aProxySrc, "callSync<Point>(\"translate\"");
    expectContains("proxy: timeout", aProxySrc,
        "callSync<std::map<std::string, std::string>, 3000000>(\"getConfig\"");
    expectContains("proxy: deprecated method", aProxy,
        "[[deprecated]]\n    [[nodiscard]] Dbusxx::Reply<void> legacy(std::int32_t code);");
    expectContains("proxy: types include", aProxy,
        "#include \"ComExampleCalcTypes.hpp\"");
    expectContains("proxy src: header include", aProxySrc,
        "#include \"CalculatorProxy.hpp\"");
    expectContains("proxy: service name guard", aProxy, "#ifndef DBUSXX_SERVICE_NAME\n#error");
    //! The well-known name is compiled in, like on the server side
    expectContains("proxy src: ctor compiles the name in", aProxySrc,
        "CalculatorProxy::CalculatorProxy()\n"
        "    : mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,");
    expectContains("proxy: ctor takes no service name", aProxy,
        "explicit CalculatorProxy();");

    section("codegen: proxy shapes (@sync / @async)");
    expectContains("proxy: async handle", aProxy, "Dbusxx::PendingReply<std::int32_t> addAsync(");
    expectContains("proxy: async callback", aProxy,
        "Dbusxx::Status addAsync(std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback,");
    expectContains("proxy: async call", aProxySrc,
        "mClient.callAsync<std::int32_t>(\"add\", std::move(aCallback), a, b)");
    expectContains("proxy: @sync signature", aProxy,
        "Dbusxx::Reply<std::int32_t> syncOnly(std::int32_t val)");
    expectContains("proxy: @sync call", aProxySrc,
        "mClient.callSync<std::int32_t>(\"syncOnly\", val)");
    expectContains("proxy: @async handle", aProxy,
        "Dbusxx::PendingReply<bool> asyncOnlyAsync(std::int32_t val)");
    expectContains("proxy: @async callback", aProxy,
        "Dbusxx::Status asyncOnlyAsync(std::function<void(Dbusxx::Reply<bool>)> aCallback,");
    expectContains("proxy: @async call", aProxySrc,
        "mClient.callAsync<bool>(\"asyncOnly\", std::move(aCallback), val)");

    section("codegen: proxy void + @timeout, signal listeners");
    expectContains("proxy: void + timeout", aProxySrc,
        "mClient.callSync<void, 500000>(\"ping\")");
    expectContains("proxy: void + timeout callback", aProxySrc,
        "mClient.callAsync<void, 500000>(\"ping\", std::move(aCallback))");
    expectContains("proxy: listener", aProxy,
        "Dbusxx::Status onValueChanged(std::function<void(std::int32_t oldVal, "
        "std::int32_t newVal)> aCallback);");
    expectContains("proxy: listener call", aProxySrc,
        "mClient.listenSignal(\"valueChanged\", std::move(aCallback))");
    expectContains("proxy: deprecated listener", aProxy,
        "[[deprecated]]\n    [[nodiscard]] Dbusxx::Status onLegacyEvent(");
    expectContains("proxy: deprecated listener call", aProxySrc,
        "mClient.listenSignal(\"legacyEvent\", std::move(aCallback))");

    section("codegen: LoggerSkeleton.hpp");
    const std::string aLogger = Codegen::genSkeletonHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[1]);
    expectContains("logger: class", aLogger,
        "class LoggerServer final : public Dbusxx::Server<LoggerServer>");
    expectContains("logger: iface", aLogger, "DBUSXX_IFACE(\"com.example.calc.Logger\")");
    expectContains("logger: signal", aLogger, "DBUSXX_SIGNAL(logAdded, std::string)");

    caseKeywordParamName();
    caseAsyncSuffixOnProperty();
    caseSyncOnlyCallbackParam();
    caseAsyncOnlyNamesAreFree();

    if (gFail != 0) {
        std::cout << "\n[RESULT] " << gFail << " check(s) FAILED\n";
        return 1;
    }

    std::cout << "\n[RESULT] pipeline checks passed\n";
    return 0;
}
