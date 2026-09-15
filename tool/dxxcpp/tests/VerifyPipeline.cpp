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
    expectContains("skeleton: new_", aSkeleton, "std::int32_t new_");
    expectContains("skeleton: class_", aSkeleton, "const std::string& class_");
    expectContains("proxy: callSync args", aProxy, "callSync<bool>(\"f\", new_, class_)");
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

    section("codegen: Types.hpp");
    const std::string aTypes = Codegen::genTypesHeader(*aSemaResult.ir);
    expectContains("types: ns", aTypes, "namespace Com::Example::Calc");
    expectContains("types: struct", aTypes, "struct Point {");
    expectContains("types: field", aTypes, "std::int32_t x;");
    expectContains("types: using", aTypes, "using ConfigMap = std::map<std::string, std::string>;");
    expectContains("types: static_assert", aTypes, "static_assert(std::is_aggregate_v<Point>");
    expectContains("types: operator==", aTypes, "bool operator==(const Point& aOther) const {");
    expectContains("types: operator== field", aTypes, "return x == aOther.x");
    expectContains("types: guard", aTypes, "#ifndef COM_EXAMPLE_CALC_TYPES_HPP");

    section("codegen: CalculatorSkeleton.hpp");
    const std::string aSkeleton = Codegen::genSkeletonHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("skel: class", aSkeleton,
        "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>");
    expectContains("skel: path", aSkeleton, "DBUSXX_PATH(\"/com/example/calc\")");
    expectContains("skel: iface", aSkeleton, "DBUSXX_IFACE(\"com.example.calc.Calculator\")");
    expectContains("skel: method", aSkeleton, "DBUSXX_METHOD(add)");
    expectContains("skel: void method with a string arg", aSkeleton, "DBUSXX_METHOD(notify)");
    expectContains("skel: struct param", aSkeleton, "const Point& p");
    expectContains("skel: signal", aSkeleton,
        "DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)");
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
    expectContains("skel: deprecated comment", aSkeleton, "// @deprecated");

    section("codegen: CalculatorProxy.hpp");
    const std::string aProxy = Codegen::genProxyHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[0]);
    expectContains("proxy: sync call", aProxy, "callSync<std::int32_t>(\"add\"");
    expectContains("proxy: struct call", aProxy, "callSync<Point>(\"translate\"");
    expectContains("proxy: timeout", aProxy,
        "callSync<std::map<std::string, std::string>, 3000000>(\"getConfig\"");
    expectContains("proxy: deprecated", aProxy, "[[deprecated]]");
    expectContains("proxy: oneway", aProxy, "callSync(\"legacy\"");

    section("codegen: LoggerSkeleton.hpp");
    const std::string aLogger = Codegen::genSkeletonHeader(*aSemaResult.ir,
        aSemaResult.ir->interfaces[1]);
    expectContains("logger: class", aLogger,
        "class LoggerServer final : public Dbusxx::Server<LoggerServer>");
    expectContains("logger: iface", aLogger, "DBUSXX_IFACE(\"com.example.calc.Logger\")");
    expectContains("logger: signal", aLogger, "DBUSXX_SIGNAL(logAdded, std::string)");

    caseKeywordParamName();

    if (gFail != 0) {
        std::cout << "\n[RESULT] " << gFail << " check(s) FAILED\n";
        return 1;
    }

    std::cout << "\n[RESULT] pipeline checks passed\n";
    return 0;
}
