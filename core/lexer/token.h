#pragma once

#include "core/util/enum.h"
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

#include "core/util/source.h"

namespace xlang {

ENUM_CLASS(TokenType, function, variable, assignment, paren_open, paren_close,
           curly_open, curly_close, identifier, string_literal, new_line,
           unknown);

struct Token {
    TokenType type;
    std::variant<std::monostate, std::string> value;
    Source source;

    Token(TokenType type, Source source) : type{type}, source{source} {}
    Token(TokenType type, std::string string, Source source)
        : type{type}, value(string), source{source} {}
};

inline auto operator<<(std::ostream& os, const Token& token) -> std::ostream& {
    os << token.type;
    switch (token.type) {
    case TokenType::identifier:
    case TokenType::string_literal:
        os << "(" << std::get<std::string>(token.value) << ")";
        break;
    default:
        break;
    }
    return os;
}

inline auto operator==(const Token& lhs, const Token& rhs) -> bool {
    return lhs.type == rhs.type && lhs.value == rhs.value;
}

} // namespace xlang
