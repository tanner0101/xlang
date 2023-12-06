#pragma once

#include <iostream>
#include <optional>
#include <string>

enum class TokenType {
    function,
    paren_open,
    paren_close,
    curly_open,
    curly_close,
    identifier,
    string_literal,
};

std::string tokenToString(TokenType tokenType) {
#define _token_type_case(name)                                                 \
    case TokenType::name:                                                      \
        return #name

    switch (tokenType) {
        _token_type_case(function);
        _token_type_case(paren_open);
        _token_type_case(paren_close);
        _token_type_case(curly_open);
        _token_type_case(curly_close);
        _token_type_case(identifier);
        _token_type_case(string_literal);
    default:
        return "unknown";
    }
}

std::ostream &operator<<(std::ostream &os, TokenType tokenType) {
    os << tokenToString(tokenType);
    return os;
}

struct Token {
    TokenType type;
    std::optional<std::string> value = std::nullopt;
};

std::ostream &operator<<(std::ostream &os, const Token &token) {
    os << token.type;
    if (token.value.has_value()) {
        os << "(" << token.value.value() << ")";
    }
    return os;
}

bool operator==(const Token &lhs, const Token &rhs) {
    return lhs.type == rhs.type && lhs.value == rhs.value;
}