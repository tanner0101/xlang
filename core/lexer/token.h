#pragma once

#include "core/util/enum.h"
#include <iostream>
#include <string>
#include <variant>

#include "core/util/source.h"

namespace xlang {

ENUM_CLASS(TokenType, function, variable, external, structure, equal, colon,
           comma, paren_open, paren_close, curly_open, curly_close, angle_open,
           angle_close, identifier, string_literal, integer_literal, new_line,
           dot, arrow, _return, variadic, unknown);

struct Token {
    TokenType type;
    std::variant<std::monostate, std::string> value;
    Source source;

    Token(TokenType type, Source source) : type{type}, source{source} {}
    Token(TokenType type, std::string string, Source source)
        : type{type}, value(string), source{source} {}

    auto operator==(const Token& other) const -> bool = default;
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
    os << " @" << token.source;
    return os;
}

} // namespace xlang
