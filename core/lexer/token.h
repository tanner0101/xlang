#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <variant>

enum class TokenType {
    function,
    paren_open,
    paren_close,
    curly_open,
    curly_close,
    identifier,
    string_literal,
    unknown,
};

inline auto tokenToString(TokenType tokenType) -> std::string {
#define TOKEN_TYPE_CASE(name)                                                  \
    case TokenType::name:                                                      \
        return #name

    switch (tokenType) {
        TOKEN_TYPE_CASE(function);
        TOKEN_TYPE_CASE(paren_open);
        TOKEN_TYPE_CASE(paren_close);
        TOKEN_TYPE_CASE(curly_open);
        TOKEN_TYPE_CASE(curly_close);
        TOKEN_TYPE_CASE(identifier);
        TOKEN_TYPE_CASE(string_literal);
    default:
        return "unknown";
    }
}

inline auto operator<<(std::ostream& os, TokenType tokenType) -> std::ostream& {
    os << tokenToString(tokenType);
    return os;
}

struct Source {
    int line;
    int column;
};

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