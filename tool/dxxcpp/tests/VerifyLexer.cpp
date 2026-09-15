//! Lexer regression: token kinds / text / line-col + error recovery
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Lexer.hpp"


namespace {
using Lexer::Kind;

int gFail = 0;
int gCase = 0;

void ok(const std::string& aWhat) {
    std::cout << "  [ OK ] " << aWhat << "\n";
}

void fail(const std::string& aWhat, const std::string& aDetail) {
    std::cout << "  [FAIL] " << aWhat << " : " << aDetail << "\n";
    ++gFail;
}

void section(const std::string& aTitle) {
    std::cout << "\n===== " << aTitle << " =====\n";
    ++gCase;
}

std::string kindOf(Kind aKind) {
    return std::string(Lexer::kindName(aKind));
}

struct Expect {
    Kind kind;
    std::string text;
};

//! Compare token by token (the expected list does not include the trailing End)
void caseTokens(const std::string& aName, const std::string& aSrc,
  const std::vector<Expect>& aExpect) {
    section(aName);
    const Lexer::Result aResult = Lexer::tokenize(aSrc);
    if (!aResult.errors.empty()) {
        fail("no lex error", "got '" + aResult.errors[0].msg + "'");
        return;
    }

    if (aResult.tokens.size() != aExpect.size() + 1 ||
        !aResult.tokens.back().checkKind(Kind::End)) {
        fail("token count", "expect " + std::to_string(aExpect.size()) +
            " + End, got " + std::to_string(aResult.tokens.size()));
        return;
    }

    for (std::size_t aIndex = 0; aIndex < aExpect.size(); ++aIndex) {
        const Lexer::Token& aToken = aResult.tokens[aIndex];
        if (aToken.kind != aExpect[aIndex].kind || aToken.text != aExpect[aIndex].text) {
            fail("token[" + std::to_string(aIndex) + "]",
                "expect " + kindOf(aExpect[aIndex].kind) + " '" + aExpect[aIndex].text +
                "', got " + kindOf(aToken.kind) + " '" + std::string(aToken.text) + "'");
            return;
        }
    }

    ok("all " + std::to_string(aExpect.size()) + " token(s) match");
}

void caseBom() {
    section("utf-8 BOM is skipped");
    const std::string aSrc = std::string("\xEF\xBB\xBF") + "package com.a;";
    const Lexer::Result aResult = Lexer::tokenize(aSrc);
    if (!aResult.errors.empty()) {
        fail("no lex error", "'" + aResult.errors[0].msg + "' at " +
            std::to_string(aResult.errors[0].line) + ":" +
            std::to_string(aResult.errors[0].col));
        return;
    }

    if (aResult.tokens.empty() || !aResult.tokens[0].checkKeyword("package") ||
        aResult.tokens[0].loc.line != 1 || aResult.tokens[0].loc.col != 1) {
        fail("BOM", "first token is not 'package' at 1:1");
        return;
    }

    ok("BOM skipped, 'package' still at 1:1");
}

//! Numeric literals: sign / dot / exponent / hex must stay a single token
void caseNumbers() {
    section("numeric literals are one token");
    const std::vector<std::pair<std::string, std::string>> aCases = {
        { "{-1}",     "-1" },
        { "{+1}",     "+1" },
        { "{1.5}",    "1.5" },
        { "{-1.5e-3}", "-1.5e-3" },
        { "{1e3}",    "1e3" },
        { "{0x1F}",   "0x1F" },
        { "{16}",     "16" },
    };

    for (const auto& aOne : aCases) {
        const std::string aSrc = "x " + aOne.first + ";";
        const Lexer::Result aResult = Lexer::tokenize(aSrc);
        if (!aResult.errors.empty()) {
            fail("literal " + aOne.first, "lex error: " + aResult.errors[0].msg);
            continue;
        }

        //! x { Number } ; End  -> 6 tokens; the 3rd must be one Number, not split
        if (aResult.tokens.size() != 6 || !aResult.tokens[2].checkKind(Kind::Number) ||
            aResult.tokens[2].text != aOne.second) {
            fail("literal " + aOne.first,
                "expect one Number('" + aOne.second + "'), got " +
                std::to_string(aResult.tokens.size()) + " token(s)");
            continue;
        }

        ok(aOne.first + " -> Number('" + aOne.second + "')");
    }
}

void caseLoc() {
    section("line/col tracking");
    const std::string aSrc = "package com.a;\ninterface I {\n    method f();\n}\n";
    const Lexer::Result aResult = Lexer::tokenize(aSrc);
    if (!aResult.errors.empty()) {
        fail("no lex error", aResult.errors[0].msg);
        return;
    }

    //! "method" on the 3rd line: line=3, col=5
    bool aFound = false;
    for (const auto& aToken : aResult.tokens) {
        if (aToken.checkKeyword("method")) {
            aFound = true;
            if (aToken.loc.line != 3 || aToken.loc.col != 5) {
                fail("method loc", "expect 3:5, got " + std::to_string(aToken.loc.line) +
                    ":" + std::to_string(aToken.loc.col));
                return;
            }
        }
    }

    if (!aFound) {
        fail("method token", "not found");
        return;
    }

    ok("'method' at 3:5");
}

void caseErrorRecovery() {
    section("error recovery (bad char + unterminated string)");
    const std::string aSrc =
        "package com.a;\nstruct S { int32 x; }\n#\nproperty p -> int32{1};\n\"abc\n";
    const Lexer::Result aResult = Lexer::tokenize(aSrc);

    if (aResult.errors.size() != 2) {
        fail("error count", "expect 2, got " + std::to_string(aResult.errors.size()));
        return;
    }

    const bool aBadChar = aResult.errors[0].msg.find("unexpected character '#'") !=
        std::string::npos && aResult.errors[0].line == 3 && aResult.errors[0].col == 1;
    const bool aUnterminated =
        aResult.errors[1].msg.find("unterminated string literal") != std::string::npos &&
        aResult.errors[1].line == 5 && aResult.errors[1].col == 1;
    if (!aBadChar || !aUnterminated) {
        fail("error message/loc", "'" + aResult.errors[0].msg + "' / '" +
            aResult.errors[1].msg + "'");
        return;
    }

    //! Scanning continues after an error: tokens after '#' must still be there
    bool aHasProperty = false;
    for (const auto& aToken : aResult.tokens) {
        if (aToken.checkKeyword("property")) {
            aHasProperty = true;
        }
    }

    if (!aHasProperty) {
        fail("recover", "'property' after '#' is missing");
        return;
    }

    ok("2 errors reported, scanning continued");
}

//! Edges: unterminated string, CRLF, tabs, empty input, BOM-only input
void caseEdges() {
    section("edge inputs");

    //! an unterminated string is reported at the opening quote
    {
        const Lexer::Result aResult = Lexer::tokenize("x \"abc");
        const bool aRejected = aResult.errors.size() == 1 &&
            aResult.errors[0].msg.find("unterminated string literal") != std::string::npos &&
            aResult.errors[0].line == 1 && aResult.errors[0].col == 3;
        if (aRejected) {
            ok("unterminated string at 1:3");
        } else {
            fail("unterminated string",
                aResult.errors.empty() ? "no error" : aResult.errors[0].msg);
        }
    }

    //! CRLF: '\r' is whitespace, only '\n' advances the line
    {
        const Lexer::Result aResult = Lexer::tokenize("package com.a;\r\ninterface I {};\r\n");
        bool aFound = false;
        for (const auto& aToken : aResult.tokens) {
            if (aToken.checkKeyword("interface")) {
                aFound = (aToken.loc.line == 2 && aToken.loc.col == 1);
            }
        }

        if (aResult.errors.empty() && aFound) {
            ok("CRLF: 'interface' at 2:1");
        } else {
            fail("CRLF", aResult.errors.empty() ? "wrong line/col" : aResult.errors[0].msg);
        }
    }

    //! a tab counts as one column
    {
        const Lexer::Result aResult = Lexer::tokenize("package\tcom.a;");
        if (aResult.errors.empty() && aResult.tokens.size() > 1 &&
            aResult.tokens[1].checkKeyword("com") && aResult.tokens[1].loc.col == 9) {
            ok("tab is one column: 'com' at 1:9");
        } else {
            fail("tab column", aResult.errors.empty() ? "unexpected loc" :
                aResult.errors[0].msg);
        }
    }

    //! empty input / BOM only -> a single End token, no error
    {
        const Lexer::Result aEmptyResult = Lexer::tokenize("");
        const Lexer::Result aBomResult = Lexer::tokenize(std::string("\xEF\xBB\xBF"));
        const bool aEmptyOk = aEmptyResult.errors.empty() && aEmptyResult.tokens.size() == 1 &&
            aEmptyResult.tokens[0].checkKind(Kind::End);
        const bool aBomOk = aBomResult.errors.empty() && aBomResult.tokens.size() == 1 &&
            aBomResult.tokens[0].checkKind(Kind::End);
        if (aEmptyOk && aBomOk) {
            ok("empty and BOM-only inputs give a single End token");
        } else {
            fail("empty/BOM", "unexpected token count or error");
        }
    }
}

} // namespace

int main() {
    //! Whole-file coverage: comments, arrow, templates, annotation, string, numbers
    {
        const std::string aSrc =
            "//! doc comment\n"
            "package com.example.calc;   // trailing comment\n"
            "struct Point { int32 x; };\n"
            "using ConfigMap = map<string, string>;\n"
            "@timeout(3000) method getConfig() -> ConfigMap;\n"
            "property version -> string{\"1.0.0\"};\n";
        caseTokens("whole file", aSrc, {
            { Kind::Iden,   "package" },
            { Kind::Iden,   "com" },
            { Kind::Dot,    "." },
            { Kind::Iden,   "example" },
            { Kind::Dot,    "." },
            { Kind::Iden,   "calc" },
            { Kind::Semi,   ";" },
            { Kind::Iden,   "struct" },
            { Kind::Iden,   "Point" },
            { Kind::LBrace, "{" },
            { Kind::Iden,   "int32" },
            { Kind::Iden,   "x" },
            { Kind::Semi,   ";" },
            { Kind::RBrace, "}" },
            { Kind::Semi,   ";" },
            { Kind::Iden,   "using" },
            { Kind::Iden,   "ConfigMap" },
            { Kind::Equal,  "=" },
            { Kind::Iden,   "map" },
            { Kind::LAngle, "<" },
            { Kind::Iden,   "string" },
            { Kind::Comma,  "," },
            { Kind::Iden,   "string" },
            { Kind::RAngle, ">" },
            { Kind::Semi,   ";" },
            { Kind::At,     "@" },
            { Kind::Iden,   "timeout" },
            { Kind::LParen, "(" },
            { Kind::Number, "3000" },
            { Kind::RParen, ")" },
            { Kind::Iden,   "method" },
            { Kind::Iden,   "getConfig" },
            { Kind::LParen, "(" },
            { Kind::RParen, ")" },
            { Kind::Arrow,  "->" },
            { Kind::Iden,   "ConfigMap" },
            { Kind::Semi,   ";" },
            { Kind::Iden,   "property" },
            { Kind::Iden,   "version" },
            { Kind::Arrow,  "->" },
            { Kind::Iden,   "string" },
            { Kind::LBrace, "{" },
            { Kind::String, "\"1.0.0\"" },
            { Kind::RBrace, "}" },
            { Kind::Semi,   ";" },
        });
    }

    //! ">>" must not be merged into one token, or nested templates cannot close
    caseTokens("nested template >>", "map<string, vector<int32>>;", {
        { Kind::Iden,   "map" },
        { Kind::LAngle, "<" },
        { Kind::Iden,   "string" },
        { Kind::Comma,  "," },
        { Kind::Iden,   "vector" },
        { Kind::LAngle, "<" },
        { Kind::Iden,   "int32" },
        { Kind::RAngle, ">" },
        { Kind::RAngle, ">" },
        { Kind::Semi,   ";" },
    });

    //! An escaped quote must not cut the string literal in half
    caseTokens("string escape", "property p -> string{\"a\\\"b\"};", {
        { Kind::Iden,   "property" },
        { Kind::Iden,   "p" },
        { Kind::Arrow,  "->" },
        { Kind::Iden,   "string" },
        { Kind::LBrace, "{" },
        { Kind::String, "\"a\\\"b\"" },
        { Kind::RBrace, "}" },
        { Kind::Semi,   ";" },
    });

    //! Arrow vs a lone '-'
    caseTokens("arrow", "method f() -> int32;", {
        { Kind::Iden,   "method" },
        { Kind::Iden,   "f" },
        { Kind::LParen, "(" },
        { Kind::RParen, ")" },
        { Kind::Arrow,  "->" },
        { Kind::Iden,   "int32" },
        { Kind::Semi,   ";" },
    });

    caseLoc();
    caseBom();
    caseNumbers();
    caseEdges();

    //! Digit separators (C++14 1'000) are unsupported: ' must be a lex error
    {
        const std::string aSrc = "x {1'000};";
        const Lexer::Result aResult = Lexer::tokenize(aSrc);
        section("digit separator is rejected");
        const bool aRejected = aResult.errors.size() == 1 &&
            aResult.errors[0].msg.find("unexpected character '''") != std::string::npos;
        if (aRejected) {
            ok("' is a lex error: " + aResult.errors[0].msg);
        } else {
            fail("1'000", aResult.errors.empty() ? "no error" : aResult.errors[0].msg);
        }
    }

    {
        const std::string aSrc = "package com.a;\nmethod f() - int32;\n";
        const Lexer::Result aResult = Lexer::tokenize(aSrc);
        const bool aBadArrow = aResult.errors.size() == 1 &&
            aResult.errors[0].msg.find("unexpected character '-'") != std::string::npos &&
            aResult.errors[0].line == 2 && aResult.errors[0].col == 12;
        if (!aBadArrow) {
            section("lone '-' is an error");
            fail("lone '-'", aResult.errors.empty() ? "no error" : aResult.errors[0].msg);
        } else {
            section("lone '-' is an error");
            ok("'unexpected character '-'' at 2:12");
        }
    }

    caseErrorRecovery();

    std::cout << "\n[RESULT] " << (gCase - gFail > 0 ? gCase - gFail : 0) << "/" << gCase
              << " case(s) passed";
    if (gFail != 0) {
        std::cout << ", " << gFail << " FAILED\n";
        return 1;
    }

    std::cout << "\n";
    return 0;
}
