#ifndef DXXCPP_LEXER_HPP
#define DXXCPP_LEXER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Ast.hpp"


namespace Lexer {
/**
 * @brief Token kind
 */
enum class Kind {
    End,    //! EOF
    Iden,   //! Identifier / Keyword
    Number, //! Numeric literal (N of array<T, N>, values like 1.5 / -1 / 1e-3; one token)
    String, //! String literal (text with quotes at both ends)
    Semi,   //! ;
    Comma,  //! ,
    Dot,    //! .
    Arrow,  //! ->
    Equal,  //! =
    At,     //! @
    LBrace, //! {
    RBrace, //! }
    LParen, //! (
    RParen, //! )
    LAngle, //! <
    RAngle  //! > (Do not merge ">>", nested templates can be closed one by one)
};

std::string_view kindName(Kind aKind);

/**
 * @brief Token
 */
struct Token {
    //! Token type
    Kind kind { Kind::End };
    //! Text at the related position in .dxx
    std::string_view text;
    //! Token started location
    Ast::Loc loc;

    bool checkKind(Kind aKind) const {
        return kind == aKind;
    }

    //! Indicate if it is a keyword of .dxx grammar
    bool checkKeyword(std::string_view aKeyword) const {
        return kind == Kind::Iden && text == aKeyword;
    }
};

struct Error {
    std::string msg;
    std::size_t line { 0 };
    std::size_t col { 0 };
};

struct Result {
    //! There must be an Token(End) at the end
    std::vector<Token> tokens;
    std::vector<Error> errors;
};

/**
 * @brief Lexical analysis: .dxx resource -> token list
 *
 * Error without interruption: After recording the error,
 *   skip the problem character and continue scanning,
 *   making it easier to report multiple errors at once.
 * Annotation ("//" and document annotations "//!") are
 *   directly discarded during the lexical stage.
 */
Result tokenize(std::string_view aSource);
} // namespace Lexer

#endif // DXXCPP_LEXER_HPP
