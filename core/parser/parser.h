#pragma once

#include "../lexer/token.h"
#include "../util/buffer.h"
#include "node.h"
#include <vector>

class Parser {
  public:
    auto parse(std::vector<Token> tokens) -> std::vector<Node> {
        return parse(Buffer<std::vector<Token>>(tokens));
    }

    auto parse(Buffer<std::vector<Token>> tokens) -> std::vector<Node>;
};
