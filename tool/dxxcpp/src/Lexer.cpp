#include "Lexer.hpp"

#include <cctype>


namespace Lexer {
namespace {
constexpr bool isIdentStart(char aChar) {
    return std::isalpha(static_cast<unsigned char>(aChar)) != 0
        || aChar == '_';
}

constexpr bool isIdentBody(char aChar) {
    return std::isalnum(static_cast<unsigned char>(aChar)) != 0
        || aChar == '_';
}

constexpr bool isDigit(char aChar) {
    return aChar >= '0' && aChar <= '9';
}

constexpr bool isSpace(char aChar) {
    return aChar == ' ' || aChar == '\t'
        || aChar == '\r' || aChar == '\n';
}

//! Scan cursor
//! Avoid going back several lines for every token taken
struct Cursor {
    std::string_view src;
    std::size_t pos { 0 };
    std::size_t line { 1 };
    std::size_t col { 1 };

    bool eof() const {
        return pos >= src.size();
    }

    char peek(std::size_t aAhead = 0) const {
        return (pos + aAhead < src.size()) ? src[pos + aAhead] : '\0';
    }

    //! Advance and auto increment for line/col
    void advance() {
        if (eof()) {
            return;
        }

        if (src[pos] == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }

        ++pos;
    }
};

//! Make token
Token makeToken(Kind aKind, const Cursor& aCur, std::size_t aStart,
  std::size_t aLine, std::size_t aCol) {
    return Token {
        aKind,
        aCur.src.substr(aStart, aCur.pos - aStart),
        Ast::Loc { aLine, aCol }
    };
}

void report(std::vector<Error>& aErrs,
  const std::string& aMsg, std::size_t aLine, std::size_t aCol) {
    aErrs.push_back(Error { aMsg, aLine, aCol });
}

//! Skip comments
void skipLineComment(Cursor& aCur) {
    while (!aCur.eof() && aCur.peek() != '\n') {
        aCur.advance();
    }
}

//! Scan string literals
void scanString(Cursor& aCur, std::vector<Error>& aErrs,
  std::size_t aLine, std::size_t aCol) {
    //! Skip quotation marks
    aCur.advance();

    bool closed = false;
    while (!aCur.eof()) {
        const char c = aCur.peek();
        //! Process \\ and \"
        if (c == '\\' && (aCur.peek(1) == '"' || aCur.peek(1) == '\\')) {
            aCur.advance();
            aCur.advance();
            continue;
        }

        //! String literal trailing quotation mark
        if (c == '"') {
            aCur.advance();
            closed = true;
            break;
        }

        //! String doesn't cross lines
        if (c == '\n') {
            break;
        }

        aCur.advance();
    }

    if (!closed) {
        report(aErrs, "unterminated string literal", aLine, aCol);
    }
}

//! Scan number literal
void scanNumber(Cursor& aCur) {
    //! Leading sign can be part of the literal (-1 / +1 / -.5)
    if (aCur.peek() == '-' || aCur.peek() == '+') {
        aCur.advance();
    }

    while (!aCur.eof()) {
        const char c = aCur.peek();
        if (isDigit(c) || isIdentBody(c) || c == '.') {
            aCur.advance();
            continue;
        }

        if (c == '+' || c == '-') {
            const char prev = (aCur.pos > 0) ? aCur.src[aCur.pos - 1] : '\0';
            if (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P') {
                aCur.advance();
                continue;
            }
        }

        break;
    }
}
} // namespace

std::string_view kindName(Kind aKind) {
    switch (aKind) {
        case Kind::End:
            return "end of file";
        case Kind::Iden:
            return "identifier";
        case Kind::Number:
            return "number";
        case Kind::String:
            return "string";
        case Kind::Semi:
            return "';'";
        case Kind::Comma:
            return "','";
        case Kind::Dot:
            return "'.'";
        case Kind::Arrow:
            return "'->'";
        case Kind::Equal:
            return "'='";
        case Kind::At:
            return "'@'";
        case Kind::LBrace:
            return "'{'";
        case Kind::RBrace:
            return "'}'";
        case Kind::LParen:
            return "'('";
        case Kind::RParen:
            return "')'";
        case Kind::LAngle:
            return "'<'";
        case Kind::RAngle:
            return "'>'";
    }

    return "unknown";
}

Result tokenize(std::string_view aSource) {
    Result res;
    Cursor cur { aSource };

    //! The "UTF-8 with BOM" editor (VS Code/Notepad) will come with a 3-byte BOM
    //! So skip them directly
    if (cur.src.size() >= 3 &&
        static_cast<unsigned char>(cur.src[0]) == 0xEF &&
        static_cast<unsigned char>(cur.src[1]) == 0xBB &&
        static_cast<unsigned char>(cur.src[2]) == 0xBF) {
        cur.pos = 3;
    }

    while (!cur.eof()) {
        const std::size_t start = cur.pos;
        const std::size_t line = cur.line;
        const std::size_t col = cur.col;
        const char c = cur.peek();

        //! Blank space
        if (isSpace(c)) {
            cur.advance();
            continue;
        }

        //! Annotation
        if (c == '/' && cur.peek(1) == '/') {
            skipLineComment(cur);
            continue;
        }

        //! Identifier / Keyword
        if (isIdentStart(c)) {
            while (isIdentBody(cur.peek())) {
                cur.advance();
            }

            res.tokens.push_back(makeToken(Kind::Iden, cur, start, line, col));
            continue;
        }

        //! Number literal: 123 / 1.5 / -1 / +1 / 1e-3 / 0x1F is one token each
        const char next = cur.peek(1);
        const bool signStart = (c == '-' || c == '+') &&
            (isDigit(next) || (next == '.' && isDigit(cur.peek(2))));
        if (isDigit(c) || (c == '.' && isDigit(next)) || signStart) {
            scanNumber(cur);
            res.tokens.push_back(makeToken(Kind::Number, cur, start, line, col));
            continue;
        }

        //! String literal 
        if (c == '"') {
            scanString(cur, res.errors, line, col);
            res.tokens.push_back(makeToken(Kind::String, cur, start, line, col));
            continue;
        }

        //! Arrow ("->")
        if (c == '-' && cur.peek(1) == '>') {
            cur.advance();
            cur.advance();
            res.tokens.push_back(makeToken(Kind::Arrow, cur, start, line, col));
            continue;
        }

        Kind kind { Kind::End };
        bool matched = true;
        switch (c) {
            case ';':
                kind = Kind::Semi;
                break;
            case ',':
                kind = Kind::Comma;
                break;
            case '.':
                kind = Kind::Dot;
                break;
            case '=':
                kind = Kind::Equal;
                break;
            case '@':
                kind = Kind::At;
                break;
            case '{':
                kind = Kind::LBrace;
                break;
            case '}':
                kind = Kind::RBrace;
                break;
            case '(':
                kind = Kind::LParen;
                break;
            case ')':
                kind = Kind::RParen;
                break;
            case '<':
                kind = Kind::LAngle;
                break;
            case '>':
                kind = Kind::RAngle;
                break;
            default:
                matched = false;
                break;
        }

        cur.advance();
        if (matched) {
            res.tokens.push_back(makeToken(kind, cur, start, line, col));
            continue;
        }

        report(res.errors, std::string("unexpected character '") + c + "'", line, col);
    }

    res.tokens.push_back(Token {
        Kind::End,
        std::string_view(),
        Ast::Loc { cur.line, cur.col }
    });

    return res;
}
} // namespace Lexer
