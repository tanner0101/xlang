#pragma once

#include "../lexer/token.h"
#include "../util/buffer.h"
#include "../util/diagnostics.h"
#include "node.h"
#include <vector>

class Parser {
  public:
    auto parse(std::vector<Token> tokens, Diagnostics& diagnostics)
        -> std::vector<Node> {
        return parse(Buffer<std::vector<Token>>(tokens), diagnostics);
    }

    auto parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
        -> std::vector<Node>;
};
