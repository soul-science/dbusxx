#ifndef DXXCPP_PARSER_HPP
#define DXXCPP_PARSER_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Ast.hpp"
#include "Lexer.hpp"


namespace Parser {

struct Error {
    std::string msg;
    std::size_t line { 0 };
    std::size_t col { 0 };
};

struct Result {
    //! When there is a syntax error, it is null opt
    std::optional<Ast::Root> root;
    //! Lexical errors & Grammatical errors
    std::vector<Error> errors;
};

Result parseTokens(const std::vector<Lexer::Token>& aTokens);

/**
 * @brief syntax analysis: .dxx -> ast
 *
 * Grammar errors will be synchronously recovered
 *   (jumping to ';' or the next top-level declaration),
 *   and try to report as many errors as possible;
 * As long as there are errors, no AST is generated
 *
 */
Result parse(std::string_view aSource);
} // namespace Parser

#endif // DXXCPP_PARSER_HPP
