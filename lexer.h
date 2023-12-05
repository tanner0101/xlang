#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <vector>

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
#define _case(name)                                                            \
    case TokenType::name:                                                      \
        return #name

    switch (tokenType) {
        _case(function);
        _case(paren_open);
        _case(paren_close);
        _case(curly_open);
        _case(curly_close);
        _case(identifier);
        _case(string_literal);
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

enum class LexerState {
    none,
    identifier,
    string_literal,
};

class StringLexer {
  private:
    std::string str;
    size_t position = 0;

  public:
    StringLexer(const std::string &s) : str(s) {}

    std::optional<char> peek() const {
        if (eof()) {
            return std::nullopt;
        }

        return str[position];
    }

    std::optional<char> pop() {
        if (eof()) {
            return std::nullopt;
        }

        return str[position++];
    }

    bool eof() const { return position >= str.size(); }
};

class Lexer {
  public:
    Lexer();
    ~Lexer();

    LexerState state = LexerState::none;

    inline std::vector<Token> lex(std::string input) {
        return lex(StringLexer(input));
    }

    std::vector<Token> lex(StringLexer input);

    void printTokens();
};
