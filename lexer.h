#pragma once

#include "buffer.h"
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
    Lexer();
    ~Lexer();

    LexerState state = LexerState::none;

    inline std::vector<Token> lex(std::string input) {
        return lex(Buffer<std::string>(input));
    }

    std::vector<Token> lex(Buffer<std::string> input);

    void printTokens();
};
