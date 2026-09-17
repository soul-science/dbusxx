#include "Parser.hpp"

#include <utility>


namespace Parser {
namespace {
using Lexer::Kind;
using Lexer::Token;

//! Nesting limit
constexpr std::size_t MAX_TYPE_NEST { 64 };
constexpr std::size_t MAX_DEFAULT_BRACE_NEST { 64 };

struct Ctx {
    const std::vector<Token>& toks;
    std::size_t idx { 0 };
    std::vector<Error> errs;

    //! Token access
    const Token& peek(std::size_t aAhead = 0) const {
        const std::size_t i = idx + aAhead;
        return (i < toks.size()) ? toks[i] : toks.back();
    }

    bool expect(Kind aKind) const {
        return peek().checkKind(aKind);
    }

    bool expect(std::string_view aKeyword) const {
        return peek().checkKeyword(aKeyword);
    }

    bool atEnd() const {
        return expect(Kind::End);
    }

    bool expectLiteral() const {
        return expect(Kind::Number) || expect(Kind::String) || expect(Kind::Iden);
    }

    std::string describe() const {
        const Token& token = peek();
        if (token.checkKind(Kind::Iden)) {
            return "'" + std::string(token.text) + "'";
        }

        return std::string(Lexer::kindName(token.kind));
    }

    const Token& consume() {
        const Token& token = peek();
        if (idx + 1 < toks.size()) {
            ++idx;
        }

        return token;
    }

    bool skipIfExpect(Kind aKind) {
        if (!expect(aKind)) {
            return false;
        }

        consume();
        return true;
    }

    bool skipIfExpect(std::string_view aKeyword) {
        if (!expect(aKeyword)) {
            return false;
        }

        consume();
        return true;
    }

    //! Report errors
    void report(const std::string& aMsg, const Ast::Loc& aLoc) {
        errs.push_back(Error { aMsg, aLoc.line, aLoc.col });
    }

    bool skipIfExpectElseReport(Kind aKind) {
        if (skipIfExpect(aKind)) {
            return true;
        }

        report("expect " + std::string(Lexer::kindName(aKind)) +
            ", but got " + describe(), peek().loc);
        return false;
    }

    bool consumeIfExpect(Kind aKind, std::string& aOut) {
        if (!expect(aKind)) {
            report("expect " + std::string(Lexer::kindName(aKind)) +
                ", but got " + describe(), peek().loc);
            return false;
        }

        aOut += consume().text;
        return true;
    }

    bool consumeAnnotationValue(std::string& aOut) {
        if (!skipIfExpectElseReport(Kind::LParen)) {
            return false;
        }

        //! @readonly() -> ""
        //! Unlike no bracket, Sema will report "doesn't require a value"
        if (skipIfExpect(Kind::RParen)) {
            aOut.clear();
            return true;
        }

        if (!expectLiteral()) {
            report("expect a literal value or " +
                std::string(Lexer::kindName(Kind::RParen)) + ", but got " + describe(),
                peek().loc);
            return false;
        }

        const std::string text(consume().text);
        if (!skipIfExpectElseReport(Kind::RParen)) {
            return false;
        }

        aOut = text;
        return true;
    }

    //! Parse a property initializer and append its text to aOut:
    //! Ex. '{' [elem (',' elem)*] '}' -> "{1, 2, 3}"
    bool consumePropertyValue(std::string& aOut, std::size_t aDepth = 0) {
        if (aDepth >= MAX_DEFAULT_BRACE_NEST) {
            report("default value nesting is too deep (max " +
                std::to_string(MAX_DEFAULT_BRACE_NEST) + ")", peek().loc);
            return false;
        }

        if (!consumeIfExpect(Kind::LBrace, aOut)) {
            return false;
        }

        if (!expect(Kind::RBrace)) {
            while (true) {
                if (!consumeInitElement(aOut, aDepth)) {
                    return false;
                }

                if (!skipIfExpect(Kind::Comma)) {
                    break;
                }

                //! The comma should be standardized as ", "
                aOut += ", ";
            }
        }

        return consumeIfExpect(Kind::RBrace, aOut);
    }

    //! Parse a property Initial value element
    //! Literal or nested '{...}'
    bool consumeInitElement(std::string& aOut, std::size_t aDepth) {
        if (expect(Kind::LBrace)) {
            return consumePropertyValue(aOut, aDepth + 1);
        }

        if (expectLiteral()) {
            aOut += consume().text;
            return true;
        }

        report("expect a literal value or '{', but got " + describe(), peek().loc);
        return false;
    }

    //! Error Recovery, skip to resume point and continue parsing
    void skipToResumePoint() {
        int depth = 0;
        while (!atEnd()) {
            if (expect(Kind::LBrace)) {
                ++depth;
                consume();
                continue;
            }

            if (expect(Kind::RBrace)) {
                --depth;
                consume();
                if (depth <= 0) {
                    skipIfExpect(Kind::Semi);
                    return;
                }

                continue;
            }

            if (depth == 0) {
                if (expect(Kind::Semi)) {
                    consume();
                    return;
                }

                if (expect("package") || expect("struct") ||
                    expect("using") || expect("interface")) {
                    return;
                }
            }

            consume();
        }
    }
};

//! Recursive parsing type
bool parseType(Ctx& aCtx, Ast::Type& aOut, std::size_t aDepth = 0) {
    if (aDepth >= MAX_TYPE_NEST) {
        aCtx.report("type nesting is too deep (max " +
            std::to_string(MAX_TYPE_NEST) + ")", aCtx.peek().loc);
        return false;
    }

    //! Parse the number (like N in array<int32, N>)
    if (aCtx.expect(Kind::Number)) {
        const Token& token = aCtx.consume();
        aOut.name = std::string(token.text);
        aOut.loc = token.loc;
        return true;
    }

    aOut.loc = aCtx.peek().loc;
    if (!aCtx.consumeIfExpect(Kind::Iden, aOut.name)) {
        return false;
    }

    if (!aCtx.skipIfExpect(Kind::LAngle)) {
        //! Return base type (Ex. int32)
        return true;
    }

    //! If the type is container (Ex. map<string, string>)
    while (true) {
        Ast::Type arg;
        if (!parseType(aCtx, arg, aDepth + 1)) {
            return false;
        }

        aOut.templateType.push_back(std::move(arg));
        if (!aCtx.skipIfExpect(Kind::Comma)) {
            break;
        }
    }

    return aCtx.skipIfExpectElseReport(Kind::RAngle);
}

//! Parse dotted name, like "com.example.calc"
bool parseDottedName(Ctx& aCtx, std::string& aOut) {
    if (!aCtx.consumeIfExpect(Kind::Iden, aOut)) {
        return false;
    }

    while (aCtx.skipIfExpect(Kind::Dot)) {
        std::string segment;
        if (!aCtx.consumeIfExpect(Kind::Iden, segment)) {
            return false;
        }

        aOut += "." + segment;
    }

    return true;
}

void parsePackage(Ctx& aCtx, Ast::Package& aOut) {
    //! Package shall locate at the beginning of file(token)
    if (!aCtx.expect("package")) {
        aCtx.report("expect 'package' declaration at the beginning of file",
            aCtx.peek().loc);
        return;
    }

    aOut.loc = aCtx.consume().loc;
    if (!parseDottedName(aCtx, aOut.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
    }
}

//! Parse inner field
bool parseField(Ctx& aCtx, Ast::Field& aOut) {
    if (!parseType(aCtx, aOut.type)) {
        return false;
    }

    aOut.loc = aOut.type.loc;
    return aCtx.consumeIfExpect(Kind::Iden, aOut.name);
}

//! Parse inner params
bool parseParams(Ctx& aCtx, std::vector<Ast::Field>& aOut) {
    if (aCtx.expect(Kind::RParen)) {
        return true;
    }

    while (true) {
        Ast::Field field;
        if (!parseField(aCtx, field)) {
            return false;
        }

        aOut.push_back(std::move(field));
        if (!aCtx.skipIfExpect(Kind::Comma)) {
            return true;
        }
    }
}

//! Parse Annotations
//! @name / @name(value)
void parseAnnotations(Ctx& aCtx, std::vector<Ast::Annotation>& aOut) {
    while (aCtx.expect(Kind::At)) {
        Ast::Annotation annotation;
        annotation.loc = aCtx.consume().loc;
        if (!aCtx.consumeIfExpect(Kind::Iden, annotation.name)) {
            return;
        }

        //! @timeout(3000) -> "3000"
        if (aCtx.expect(Kind::LParen)) {
            std::string value;
            if (!aCtx.consumeAnnotationValue(value)) {
                return;
            }

            annotation.value = std::move(value);
        }

        aOut.push_back(std::move(annotation));
    }
}

void parseStruct(Ctx& aCtx, Ast::Root& aRoot, const Ast::Loc& aLoc) {
    Ast::StructType st;
    st.loc = aLoc;
    if (!aCtx.consumeIfExpect(Kind::Iden, st.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::LBrace)) {
        aCtx.skipToResumePoint();
        return;
    }

    while (!aCtx.expect(Kind::RBrace) && !aCtx.atEnd()) {
        Ast::Field field;
        if (!parseField(aCtx, field)) {
            aCtx.skipToResumePoint();
            return;
        }

        if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
            aCtx.skipToResumePoint();
            return;
        }

        st.fields.push_back(std::move(field));
    }

    if (!aCtx.skipIfExpectElseReport(Kind::RBrace)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aRoot.structs.push_back(std::move(st));
}

void parseAlias(Ctx& aCtx, Ast::Root& aRoot, const Ast::Loc& aLoc) {
    Ast::AliasType alias;
    alias.loc = aLoc;
    if (!aCtx.consumeIfExpect(Kind::Iden, alias.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Equal)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!parseType(aCtx, alias.targetType)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aRoot.alias.push_back(std::move(alias));
}

void parseMethod(Ctx& aCtx, Ast::Interface& aIfce, const Ast::Loc& aLoc,
  std::vector<Ast::Annotation>&& aAnns) {
    //! Ex. add(int32 a, int32 b) -> int32
    Ast::Method method;
    method.loc = aLoc;
    method.annotations = std::move(aAnns);

    //! Parse "add"
    if (!aCtx.consumeIfExpect(Kind::Iden, method.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    //! Parse "("
    if (!aCtx.skipIfExpectElseReport(Kind::LParen)) {
        aCtx.skipToResumePoint();
        return;
    }

    //! Parse "int32 a, int32 b"
    if (!parseParams(aCtx, method.params)) {
        aCtx.skipToResumePoint();
        return;
    }

    //! Parse ")"
    if (!aCtx.skipIfExpectElseReport(Kind::RParen)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (aCtx.skipIfExpect(Kind::Arrow)) {
        Ast::Type ret;
        if (!parseType(aCtx, ret)) {
            aCtx.skipToResumePoint();
            return;
        }

        method.retType = std::move(ret);
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aIfce.methods.push_back(std::move(method));
}

void parseSignal(Ctx& aCtx, Ast::Interface& aIfce, const Ast::Loc& aLoc,
  std::vector<Ast::Annotation>&& aAnns) {
    Ast::Signal signal;
    signal.loc = aLoc;
    signal.annotations = std::move(aAnns);
    if (!aCtx.consumeIfExpect(Kind::Iden, signal.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::LParen)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!parseParams(aCtx, signal.params)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::RParen)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aIfce.signals.push_back(std::move(signal));
}

void parseProperty(Ctx& aCtx, Ast::Interface& aIfce, const Ast::Loc& aLoc,
  std::vector<Ast::Annotation>&& aAnns) {
    Ast::Property property;
    property.loc = aLoc;
    property.annotations = std::move(aAnns);
    if (!aCtx.consumeIfExpect(Kind::Iden, property.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Arrow)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!parseType(aCtx, property.type)) {
        aCtx.skipToResumePoint();
        return;
    }

    //! 'property p -> string{"1.0.0"}' -> '{"1.0.0"}'
    if (aCtx.expect(Kind::LBrace)) {
        if (!aCtx.consumePropertyValue(property.defaultValue)) {
            aCtx.skipToResumePoint();
            return;
        }
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aIfce.properties.push_back(std::move(property));
}

void parseInterface(Ctx& aCtx, Ast::Root& aRoot, const Ast::Loc& aLoc) {
    Ast::Interface ifce;
    ifce.loc = aLoc;
    if (!aCtx.consumeIfExpect(Kind::Iden, ifce.name)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::LBrace)) {
        aCtx.skipToResumePoint();
        return;
    }

    while (!aCtx.expect(Kind::RBrace) && !aCtx.atEnd()) {
        //! Parse annotation
        std::vector<Ast::Annotation> anns;
        parseAnnotations(aCtx, anns);

        const Ast::Loc loc = aCtx.peek().loc;
        if (aCtx.skipIfExpect("method")) {
            parseMethod(aCtx, ifce, loc, std::move(anns));
        }
        else if (aCtx.skipIfExpect("signal")) {
            parseSignal(aCtx, ifce, loc, std::move(anns));
        }
        else if (aCtx.skipIfExpect("property")) {
            parseProperty(aCtx, ifce, loc, std::move(anns));
        }
        else {
            aCtx.report("expect 'method' / 'signal' / 'property', but got " +
                aCtx.describe(), loc);
            aCtx.skipToResumePoint();
            return;
        }
    }

    if (!aCtx.skipIfExpectElseReport(Kind::RBrace)) {
        aCtx.skipToResumePoint();
        return;
    }

    if (!aCtx.skipIfExpectElseReport(Kind::Semi)) {
        aCtx.skipToResumePoint();
        return;
    }

    aRoot.interfaces.push_back(std::move(ifce));
}

Ast::Root parseRoot(Ctx& aCtx) {
    Ast::Root root;
    //! Parse package
    parsePackage(aCtx, root.package);
    //! Parse others
    while (!aCtx.atEnd()) {
        const Ast::Loc loc = aCtx.peek().loc;
        //! Parse struct
        if (aCtx.skipIfExpect("struct")) {
            parseStruct(aCtx, root, loc);
        }
        //! Parse alias
        else if (aCtx.skipIfExpect("using")) {
            parseAlias(aCtx, root, loc);
        }
        //! Parse interface
        else if (aCtx.skipIfExpect("interface")) {
            parseInterface(aCtx, root, loc);
        }
        else if (aCtx.skipIfExpect("package")) {
            aCtx.report("duplicate 'package' declaration", loc);
            aCtx.skipToResumePoint();
        }
        else {
            aCtx.report("expect 'struct' / 'using' / 'interface', but got " +
                aCtx.describe(), loc);
            aCtx.skipToResumePoint();
        }
    }

    return root;
}
} // namespace

Result parseTokens(const std::vector<Lexer::Token>& aTokens) {
    Result res;
    if (aTokens.empty() || !aTokens.back().checkKind(Kind::End)) {
        res.errors.push_back(Error {
            "token stream is not terminated by end of file", 0, 0
        });
        return res;
    }

    Ctx ctx { aTokens };
    Ast::Root root = parseRoot(ctx);
    if (ctx.errs.empty()) {
        res.root = std::move(root);
    }

    res.errors = std::move(ctx.errs);
    return res;
}

Result parse(std::string_view aSource) {
    //! Lexical analysis: .dxx -> token
    const Lexer::Result lex = Lexer::tokenize(aSource);
    //! Syntax analysis: token -> ast
    Result res = parseTokens(lex.tokens);
    if (!lex.errors.empty()) {
        std::vector<Error> errors;
        errors.reserve(lex.errors.size() + res.errors.size());
        for (const auto& e : lex.errors) {
            errors.push_back(Error { e.msg, e.line, e.col });
        }

        errors.insert(errors.end(), res.errors.begin(), res.errors.end());
        res.errors = std::move(errors);
        res.root.reset();
    }

    return res;
}
} // namespace Parser
