#pragma once

#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include "core/util/enum.h"
#include "token.h"
#include <optional>
#include <string>
#include <vector>

namespace xlang {

ENUM_CLASS(LexerState, none, identifier, string_literal, integer_literal);

class Lexer {
  public:
    Lexer() = default;
    ~Lexer() = default;

    LexerState state = LexerState::none;

    inline auto lex(std::string input, Diagnostics& diagnostics)
        -> std::vector<Token> {
        return lex(Buffer<std::string>(input), diagnostics);
    }

    auto lex(Buffer<std::string> input, Diagnostics& diagnostics)
        -> std::vector<Token>;
};

} // namespace xlang
