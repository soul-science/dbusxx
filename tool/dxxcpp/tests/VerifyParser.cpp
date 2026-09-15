//! Parser regression: positive (whole .dxx AST shape) + negative (errors, recovery)
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Parser.hpp"


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

void expectEq(const std::string& aWhat, const std::string& aActual,
              const std::string& aExpected) {
    if (aActual == aExpected) {
        ok(aWhat + " = '" + aActual + "'");
        return;
    }

    fail(aWhat, "expect '" + aExpected + "', got '" + aActual + "'");
}

void expectEq(const std::string& aWhat, std::size_t aActual, std::size_t aExpected) {
    if (aActual == aExpected) {
        ok(aWhat + " = " + std::to_string(aActual));
        return;
    }

    fail(aWhat, "expect " + std::to_string(aExpected) + ", got " + std::to_string(aActual));
}

//! Readable reconstruction of a type expression: vector<Point> / array<int32, 16>
std::string typeText(const Ast::Type& aType) {
    if (aType.templateType.empty()) {
        return aType.name;
    }

    std::string aOut = aType.name + "<";
    for (std::size_t aIndex = 0; aIndex < aType.templateType.size(); ++aIndex) {
        if (aIndex != 0) {
            aOut += ", ";
        }

        aOut += typeText(aType.templateType[aIndex]);
    }

    return aOut + ">";
}

//! Build deeply nested type source: vector<vector<...<int32>...>>
std::string deepTypeSrc(std::size_t aDepth) {
    std::string aTypeText;
    for (std::size_t aIndex = 0; aIndex < aDepth; ++aIndex) {
        aTypeText += "vector<";
    }

    aTypeText += "int32";
    for (std::size_t aIndex = 0; aIndex < aDepth; ++aIndex) {
        aTypeText += ">";
    }

    return "package com.example.deep;\nusing A = " + aTypeText + ";\n";
}

//! Build a deeply nested initializer: property p -> vector<int32>{{{...1...}}}
std::string deepInitSrc(std::size_t aDepth) {
    return "package com.example.deep;\ninterface I { property p -> vector<int32>" +
        std::string(aDepth, '{') + "1" + std::string(aDepth, '}') + "; };\n";
}

//! Mirrors the full example of note/dxx.md (codegen-only parts removed)
const char* FULL_EXAMPLE = R"(//! package declaration (becomes the namespace)
package com.example.calc;

//! custom types
struct Point {
    int32 x;
    int32 y;
};

struct Rectangle {
    Point topLeft;
    Point bottomRight;
};

//! type aliases
using UserId = int32;
using ConfigMap = map<string, string>;

//! interface definition
interface Calculator {
    //! basic arithmetic
    method add(int32 a, int32 b) -> int32;
    method multiply(int32 a, int32 b) -> int32;

    //! struct parameter
    method translate(Point p, int32 dx, int32 dy) -> Point;

    //! container + struct mix
    method transformPolyline(vector<Point> points, int32 dx, int32 dy) -> vector<Point>;

    //! deprecated + no return value (void)
    @deprecated method notify(string msg);

    //! timeout control
    @timeout(3000) method getConfig() -> ConfigMap;

    //! sync-only method
    @sync method syncOnly(int32 val) -> int32;

    //! ---------- signals ----------
    signal valueChanged(int32 oldVal, int32 newVal);
    signal pointMoved(Point oldPos, Point newPos);
    signal errorOccurred(int32 code, string msg);

    //! ---------- properties ----------
    @readonly property version -> string{"1.0.0"};
    property counter -> int32{0};
    property label -> string{"default"};
};
)";

void caseFullExample() {
    section("full example -> AST");
    const Parser::Result aParserResult = Parser::parse(FULL_EXAMPLE);
    if (!aParserResult.errors.empty()) {
        fail("no parse error", "'" + aParserResult.errors[0].msg + "' at " +
            std::to_string(aParserResult.errors[0].line) + ":" +
            std::to_string(aParserResult.errors[0].col));
        return;
    }

    if (!aParserResult.root) {
        fail("root", "nullopt without errors");
        return;
    }

    const Ast::Root& aRoot = *aParserResult.root;
    expectEq("package", aRoot.package.name, "com.example.calc");
    expectEq("package line", aRoot.package.loc.line, 2);
    expectEq("struct count", aRoot.structs.size(), 2);
    expectEq("alias count", aRoot.alias.size(), 2);
    expectEq("interface count", aRoot.interfaces.size(), 1);

    if (aRoot.structs.size() < 2 || aRoot.alias.size() < 2 || aRoot.interfaces.size() < 1) {
        return;
    }

    expectEq("struct[0].name", aRoot.structs[0].name, "Point");
    expectEq("struct[0].fields", aRoot.structs[0].fields.size(), 2);
    if (aRoot.structs[0].fields.size() != 2 || aRoot.structs[1].fields.size() != 2) {
        fail("struct fields", "unexpected field count");
        return;
    }

    expectEq("struct[0].field[0].type", typeText(aRoot.structs[0].fields[0].type), "int32");
    expectEq("struct[0].field[0].name", aRoot.structs[0].fields[0].name, "x");
    expectEq("struct[0].field[1].name", aRoot.structs[0].fields[1].name, "y");
    expectEq("struct[1].name", aRoot.structs[1].name, "Rectangle");
    expectEq("struct[1].field[0].type", typeText(aRoot.structs[1].fields[0].type), "Point");
    expectEq("struct[0] line", aRoot.structs[0].loc.line, 5);

    expectEq("alias[0].name", aRoot.alias[0].name, "UserId");
    expectEq("alias[0].target", typeText(aRoot.alias[0].targetType), "int32");
    expectEq("alias[1].name", aRoot.alias[1].name, "ConfigMap");
    expectEq("alias[1].target", typeText(aRoot.alias[1].targetType), "map<string, string>");
    expectEq("alias[1] line", aRoot.alias[1].loc.line, 17);

    const Ast::Interface& aInterface = aRoot.interfaces[0];
    expectEq("interface.name", aInterface.name, "Calculator");
    expectEq("methods", aInterface.methods.size(), 7);
    expectEq("signals", aInterface.signals.size(), 3);
    expectEq("properties", aInterface.properties.size(), 3);
    if (aInterface.methods.size() < 7 || aInterface.signals.size() < 3 ||
        aInterface.properties.size() < 3) {
        return;
    }

    //! add(int32 a, int32 b) -> int32
    const Ast::Method& aAdd = aInterface.methods[0];
    expectEq("add.name", aAdd.name, "add");
    expectEq("add.params", aAdd.params.size(), 2);
    expectEq("add.param[0].type", typeText(aAdd.params[0].type), "int32");
    expectEq("add.param[0].name", aAdd.params[0].name, "a");
    expectEq("add.ret", aAdd.retType ? typeText(*aAdd.retType) : std::string("(void)"), "int32");
    expectEq("add line", aAdd.loc.line, 22);

    //! translate(Point p, int32 dx, int32 dy) -> Point
    const Ast::Method& aTranslate = aInterface.methods[2];
    expectEq("translate.param[0].type", typeText(aTranslate.params[0].type), "Point");
    expectEq("translate.params", aTranslate.params.size(), 3);
    expectEq("translate.ret",
        aTranslate.retType ? typeText(*aTranslate.retType) : "(void)", "Point");

    //! vector<Point> parameter + return value
    const Ast::Method& aPolyline = aInterface.methods[3];
    expectEq("polyline.param[0].type", typeText(aPolyline.params[0].type), "vector<Point>");
    expectEq("polyline.ret", aPolyline.retType ? typeText(*aPolyline.retType) : "(void)",
        "vector<Point>");

    //! @deprecated method notify(string msg);   (no return value)
    const Ast::Method& aNotify = aInterface.methods[4];
    expectEq("notify.name", aNotify.name, "notify");
    expectEq("notify.ret", std::string(aNotify.retType ? "has" : "void"), "void");
    expectEq("notify.annotations", aNotify.annotations.size(), 1);
    expectEq("notify.annotation[0]", aNotify.annotations[0].name, "deprecated");
    expectEq("notify.annotation[0].value",
        std::string(aNotify.annotations[0].value ? "has" : "none"), "none");
    expectEq("notify line", aNotify.loc.line, 32);

    //! @timeout(3000) -> the annotation value is the raw fragment "3000"
    const Ast::Method& aConfig = aInterface.methods[5];
    expectEq("getConfig.annotation[0]", aConfig.annotations[0].name, "timeout");
    expectEq("getConfig.annotation[0].value",
        aConfig.annotations[0].value ? *aConfig.annotations[0].value : std::string("(none)"),
        "3000");
    expectEq("getConfig.ret",
        aConfig.retType ? typeText(*aConfig.retType) : "(void)", "ConfigMap");

    //! @sync
    expectEq("syncOnly.annotations", aInterface.methods[6].annotations.size(), 1);
    expectEq("syncOnly.annotation[0]", aInterface.methods[6].annotations[0].name, "sync");

    //! signal pointMoved(Point oldPos, Point newPos);
    const Ast::Signal& aMoved = aInterface.signals[1];
    expectEq("pointMoved.name", aMoved.name, "pointMoved");
    expectEq("pointMoved.params", aMoved.params.size(), 2);
    expectEq("pointMoved.param[1].name", aMoved.params[1].name, "newPos");

    //! @readonly property version -> string{"1.0.0"};   (raw text, braces kept)
    const Ast::Property& aVersion = aInterface.properties[0];
    expectEq("version.name", aVersion.name, "version");
    expectEq("version.type", typeText(aVersion.type), "string");
    expectEq("version.defaultValue", aVersion.defaultValue, "{\"1.0.0\"}");
    expectEq("version.annotations", aVersion.annotations.size(), 1);
    expectEq("version.annotation[0]", aVersion.annotations[0].name, "readonly");

    expectEq("counter.defaultValue", aInterface.properties[1].defaultValue, "{0}");
    expectEq("label.defaultValue", aInterface.properties[2].defaultValue, "{\"default\"}");
}

void caseTypes() {
    section("array<T, N> / nested templates / empty container value");
    const std::string aSrc =
        "package com.example.t;\n"
        "struct S {\n"
        "    array<int32, 16> buf;\n"
        "};\n"
        "using M = map<string, vector<int32>>;\n"
        "interface I {\n"
        "    method f(map<string, array<string, 4>> m) -> vector<vector<Point>>;\n"
        "    property metadata -> map<string, string>{};\n"
        "    property empty -> string{};\n"
        "};\n";
    const Parser::Result aParserResult = Parser::parse(aSrc);
    if (!aParserResult.errors.empty() || !aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    //! a stack struct must parse as well
    const Ast::Root& aRoot = *aParserResult.root;
    expectEq("struct count", aRoot.structs.size(), 1);
    if (aRoot.structs.size() != 1 || aRoot.structs[0].fields.size() != 1) {
        return;
    }

    const Ast::Type& aBuf = aRoot.structs[0].fields[0].type;
    expectEq("array type", typeText(aBuf), "array<int32, 16>");
    if (aBuf.templateType.size() != 2) {
        fail("array args",
            "expect 2 template args, got " + std::to_string(aBuf.templateType.size()));
        return;
    }

    expectEq("array element", typeText(aBuf.templateType[0]), "int32");
    //! N is a number leaf: name holds the digits, templateType stays empty
    expectEq("array size name", aBuf.templateType[1].name, "16");
    expectEq("array size leaf", aBuf.templateType[1].templateType.size(), 0);

    expectEq("alias target", typeText(aRoot.alias[0].targetType), "map<string, vector<int32>>");

    const Ast::Interface& aInterface = aRoot.interfaces[0];
    if (aInterface.methods.size() != 1 || aInterface.properties.size() != 2) {
        fail("ifce members", "unexpected member count");
        return;
    }

    expectEq("nested param", typeText(aInterface.methods[0].params[0].type),
        "map<string, array<string, 4>>");
    expectEq("nested ret", typeText(*aInterface.methods[0].retType), "vector<vector<Point>>");
    //! "{}" -> the initializer text is "{}" (only a missing brace pair gives "")
    expectEq("map default empty", aInterface.properties[0].defaultValue, "{}");
    expectEq("string default empty", aInterface.properties[1].defaultValue, "{}");
}

//! Nesting below the limit passes (limit is 64)
void caseModerateNesting() {
    section("moderate nesting (32 levels) is fine");
    const Parser::Result aParserResult = Parser::parse(deepTypeSrc(32));
    if (!aParserResult.errors.empty() || !aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    expectEq("aliases", aParserResult.root->alias.size(), 1);
    ok("32-level nested type parsed");
}

//! Nesting budgets are exact: the type parser trips at 64 levels, the
//! initializer parser at 65 (its outermost brace pair is depth 0)
void caseNestingLimit() {
    section("nesting limit boundary");

    const Parser::Result aTypeOk = Parser::parse(deepTypeSrc(63));
    if (!aTypeOk.errors.empty() || !aTypeOk.root) {
        fail("type 63", aTypeOk.errors.empty() ? "no root" : aTypeOk.errors[0].msg);
    } else {
        ok("63-level nested type accepted");
    }

    const Parser::Result aTypeOver = Parser::parse(deepTypeSrc(64));
    if (aTypeOver.root || aTypeOver.errors.empty() ||
        aTypeOver.errors[0].msg.find("type nesting is too deep") == std::string::npos) {
        fail("type 64", aTypeOver.errors.empty() ? "no error" : aTypeOver.errors[0].msg);
    } else {
        ok("64-level nested type rejected");
    }

    const Parser::Result aInitOk = Parser::parse(deepInitSrc(64));
    if (!aInitOk.errors.empty() || !aInitOk.root) {
        fail("init 64", aInitOk.errors.empty() ? "no root" : aInitOk.errors[0].msg);
    } else {
        ok("64-level nested initializer accepted");
    }

    const Parser::Result aInitOver = Parser::parse(deepInitSrc(65));
    if (aInitOver.root || aInitOver.errors.empty() ||
        aInitOver.errors[0].msg.find("default value nesting is too deep") ==
            std::string::npos) {
        fail("init 65", aInitOver.errors.empty() ? "no error" : aInitOver.errors[0].msg);
    } else {
        ok("65-level nested initializer rejected");
    }
}

//! parseTokens() defends against a malformed token stream (no trailing End)
void caseTokenStreamTerminator() {
    section("token stream without End");
    const Parser::Result aEmptyResult = Parser::parseTokens({});
    if (!aEmptyResult.root && !aEmptyResult.errors.empty() &&
        aEmptyResult.errors[0].msg.find("not terminated by end of file") != std::string::npos) {
        ok("empty token stream rejected: " + aEmptyResult.errors[0].msg);
    } else {
        fail("empty stream", aEmptyResult.errors.empty() ? "no error" :
            aEmptyResult.errors[0].msg);
    }

    Lexer::Token aIden;
    aIden.kind = Lexer::Kind::Iden;
    aIden.text = "package";
    const Parser::Result aNoEndResult = Parser::parseTokens({ aIden });
    if (!aNoEndResult.root && !aNoEndResult.errors.empty() &&
        aNoEndResult.errors[0].msg.find("not terminated by end of file") != std::string::npos) {
        ok("stream without End rejected: " + aNoEndResult.errors[0].msg);
    } else {
        fail("no End", aNoEndResult.errors.empty() ? "no error" :
            aNoEndResult.errors[0].msg);
    }
}

void caseEmptyInterfaceAndStruct() {
    section("empty interface / struct");
    const Parser::Result aParserResult = Parser::parse(
        "package com.example.e;\ninterface Calculator {};\nstruct Empty {};\n");
    if (!aParserResult.errors.empty() || !aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    expectEq("interfaces", aParserResult.root->interfaces.size(), 1);
    expectEq("structs", aParserResult.root->structs.size(), 1);
    expectEq("interface methods", aParserResult.root->interfaces[0].methods.size(), 0);
    expectEq("struct fields", aParserResult.root->structs[0].fields.size(), 0);
}

//! Missing ';': the declaration is incomplete -> not in the AST, exactly one error
void caseMissingSemi() {
    section("missing ';' drops the declaration, reports once");
    const Parser::Result aParserResult = Parser::parse(
        "package com.m;\nstruct S { int32 x; } int32 garbage;\n");
    if (aParserResult.root) {
        fail("root", "expect no AST when syntax error");
        return;
    }

    if (aParserResult.errors.size() != 1) {
        std::cout << "        actual errors:\n";
        for (const auto& aError : aParserResult.errors) {
            std::cout << "        " << aError.line << ":" << aError.col << " "
                      << aError.msg << "\n";
        }

        fail("error count", "expect 1, got " +
            std::to_string(aParserResult.errors.size()));
        return;
    }

    if (aParserResult.errors[0].msg.find("expect ';'") == std::string::npos) {
        fail("error message", "'" + aParserResult.errors[0].msg + "'");
        return;
    }

    ok("exactly 1 error: " + aParserResult.errors[0].msg);
}

//! Numeric initializers: sign / float / exponent must be taken verbatim (one Number token)
void caseNumericDefault() {
    section("numeric default values");
    const Parser::Result aParserResult = Parser::parse(
        "package com.n;\ninterface I {\n"
        "    property a -> int32{-1};\n"
        "    property b -> double{1.5};\n"
        "    property c -> double{1e-3};\n"
        "};\n");
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Ast::Interface& aInterface = aParserResult.root->interfaces[0];
    if (aInterface.properties.size() != 3) {
        fail("properties", "expect 3");
        return;
    }

    expectEq("a.defaultValue", aInterface.properties[0].defaultValue, "{-1}");
    expectEq("b.defaultValue", aInterface.properties[1].defaultValue, "{1.5}");
    expectEq("c.defaultValue", aInterface.properties[2].defaultValue, "{1e-3}");
}

//! Multi-value initializers: 1..N elements, nested braces; tokens join with ", "
void caseMultiValueDefault() {
    section("multi-value default initializers");
    const Parser::Result aParserResult = Parser::parse(
        "package com.m;\ninterface I {\n"
        "    property a -> vector<int32>{1,2,3};\n"
        "    property b -> vector<Point>{ {1, 2} , {3,4} };\n"
        "    property c -> map<string, int32>{{ \"x\" , 1 }};\n"
        "    property d -> vector<int32>{};\n"
        "    property e -> Point{1, 2};\n"
        "    property f -> bool{true};\n"
        "};\n");
    if (!aParserResult.root) {
        fail("parse", aParserResult.errors.empty() ? "no root" : aParserResult.errors[0].msg);
        return;
    }

    const Ast::Interface& aInterface = aParserResult.root->interfaces[0];
    if (aInterface.properties.size() != 6) {
        fail("properties", "expect 6, got " + std::to_string(aInterface.properties.size()));
        return;
    }

    expectEq("a.defaultValue", aInterface.properties[0].defaultValue, "{1, 2, 3}");
    expectEq("b.defaultValue", aInterface.properties[1].defaultValue, "{{1, 2}, {3, 4}}");
    expectEq("c.defaultValue", aInterface.properties[2].defaultValue, "{{\"x\", 1}}");
    expectEq("d.defaultValue", aInterface.properties[3].defaultValue, "{}");
    expectEq("e.defaultValue", aInterface.properties[4].defaultValue, "{1, 2}");
    expectEq("f.defaultValue", aInterface.properties[5].defaultValue, "{true}");
}

struct BadCase {
    const char* name;
    std::string src;
    std::vector<std::string> needles;   //!< error messages must contain these parts
    std::size_t minErrors { 1 };
};

void caseErrors() {
    section("negative cases");
    const std::vector<BadCase> aCases = {
        { "missing package",
          "struct Point { int32 x; };\n",
          { "expect 'package' declaration" }, 1 },
        { "package not first",
          "//! c\ninterface I {};\n",
          { "expect 'package' declaration" }, 1 },
        { "duplicate package",
          "package com.a;\npackage com.b;\ninterface I {};\n",
          { "duplicate 'package' declaration" }, 1 },
        { "missing ';' after field",
          "package com.a;\nstruct S { int32 x\n};\n",
          { "expect ';'" }, 1 },
        { "missing ';' after struct",
          "package com.a;\nstruct S {};\ninterface I {}\n",
          { "expect ';'" }, 1 },
        { "missing '{' after struct name",
          "package com.a;\nstruct S;\n",
          { "expect '{'" }, 1 },
        { "unknown member in interface",
          "package com.a;\ninterface I { foo bar; };\n",
          { "expect 'method' / 'signal' / 'property'" }, 1 },
        { "property without '->'",
          "package com.a;\ninterface I { property p int32{1}; };\n",
          { "expect '->'" }, 1 },
        { "annotation without member",
          "package com.a;\ninterface I { @readonly }; \n",
          { "expect 'method' / 'signal' / 'property'" }, 1 },
        { "unclosed struct",
          "package com.a;\nstruct S { int32 x;\n",
          { "expect '}'" }, 1 },
        { "unknown top level keyword",
          "package com.a;\nclass Foo {};\nstruct S {};\n",
          { "expect 'struct' / 'using' / 'interface'" }, 1 },
        { "bad identifier",
          "package com.a;\nstruct 42 {};\n",
          { "expect identifier" }, 1 },
        //! Recovery: both errors must be reported
        { "two errors recovered",
          "package com.a;\nstruct A { int32 x }\nstruct B { int32 y; };\ninterface I { junk; };\n",
          { "expect ';'", "expect 'method' / 'signal' / 'property'" }, 2 },
        //! a comma is missing between two elements
        { "default value missing comma",
          "package com.a;\ninterface I { property q -> vector<string>{\"a\" \"b\"}; };\n",
          { "expect '}'" }, 1 },
        //! empty element (bare comma)
        { "default value empty element",
          "package com.a;\ninterface I { property q -> vector<string>{,}; };\n",
          { "expect a literal value" }, 1 },
        //! trailing comma: C++ allows it, .dxx rejects it to stay consistent
        { "default value trailing comma",
          "package com.a;\ninterface I { property q -> vector<int32>{1, 2,}; };\n",
          { "expect a literal value" }, 1 },
        //! the first token of the value is not a literal
        { "non-literal default value",
          "package com.a;\ninterface I { property q -> string{@x}; };\n",
          { "expect a literal value" }, 1 },
        //! type nesting limit: otherwise Sema/Codegen recursion overflows (real SIGSEGV)
        { "type nesting too deep", deepTypeSrc(200),
          { "type nesting is too deep" }, 1 },
        //! initializer nesting limit: otherwise consumePropertyValue recurses too deep
        { "default value nesting too deep", deepInitSrc(200),
          { "default value nesting is too deep" }, 1 },
    };

    for (const auto& aCase : aCases) {
        const Parser::Result aParserResult = Parser::parse(aCase.src);
        if (aParserResult.root) {
            fail(aCase.name, "expected failure, but AST was produced");
            continue;
        }

        if (aParserResult.errors.size() < aCase.minErrors) {
            fail(aCase.name, "expect >= " + std::to_string(aCase.minErrors) + " error(s), got " +
                std::to_string(aParserResult.errors.size()));
            continue;
        }

        bool aAllFound = true;
        for (const auto& aNeedle : aCase.needles) {
            bool aFound = false;
            for (const auto& aError : aParserResult.errors) {
                if (aError.msg.find(aNeedle) != std::string::npos) {
                    aFound = true;
                    break;
                }
            }

            if (!aFound) {
                std::cout << "        missing '" << aNeedle << "'. actual:\n";
                for (const auto& aError : aParserResult.errors) {
                    std::cout << "        " << aError.line << ":" << aError.col << " "
                              << aError.msg << "\n";
                }

                aAllFound = false;
            }
        }

        if (!aAllFound) {
            ++gFail;
            continue;
        }

        ok(std::string(aCase.name) + " (" + std::to_string(aParserResult.errors.size()) +
            " error(s))");
    }
}

} // namespace

int main() {
    caseFullExample();
    caseTypes();
    caseModerateNesting();
    caseNestingLimit();
    caseEmptyInterfaceAndStruct();
    caseMissingSemi();
    caseNumericDefault();
    caseMultiValueDefault();
    caseTokenStreamTerminator();
    caseErrors();

    if (gFail != 0) {
        std::cout << "\n[RESULT] " << gFail << " check(s) FAILED\n";
        return 1;
    }

    std::cout << "\n[RESULT] all parser checks passed\n";
    return 0;
}
