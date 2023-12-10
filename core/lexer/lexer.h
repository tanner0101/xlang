#pragma once

#include "../util/buffer.h"
#include "token.h"
#include <optional>
#include <string>
#include <vector>

enum class LexerState {
    none,
    identifier,
    string_literal,
};

class Lexer {
  public:
    Lexer() = default;
    ~Lexer() = default;

    LexerState state = LexerState::none;

    inline auto lex(std::string input) -> std::vector<Token> {
        return lex(Buffer<std::string>(input));
    }

    auto lex(Buffer<std::string> input) -> std::vector<Token>;

    void printTokens();
};
