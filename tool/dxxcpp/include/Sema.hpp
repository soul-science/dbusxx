#ifndef DXXCPP_SEMA_HPP
#define DXXCPP_SEMA_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Ast.hpp"
#include "Ir.hpp"


namespace Sema {
struct Error {
    std::string msg;
    std::size_t line;
    std::size_t col;
};

struct Result {
    std::optional<Ir::Root> ir;
    std::vector<Error> errors;
};

std::vector<Error> validateAst(const Ast::Root& aAstRoot);

Result generateIr(const Ast::Root& aAstRoot);

Result analyze(const Ast::Root& aAstRoot);
}

#endif
