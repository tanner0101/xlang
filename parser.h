#pragma once

#include "ast_node.h"
#include "buffer.h"
#include "token.h"
#include <vector>

class Parser {
  public:
    Parser();
    ~Parser();

    std::vector<ASTNode> parse(std::vector<Token> tokens) {
        return parse(Buffer<std::vector<Token>>(tokens));
    }

    std::vector<ASTNode> parse(Buffer<std::vector<Token>> tokens);
};
