#pragma once

#include "core/lexer/token.h"
#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include "node.h"
#include <vector>

namespace xlang {

class Parser {
  public:
    auto parse(std::vector<Token> tokens, Diagnostics& diagnostics)
        -> std::vector<Node> {
        return parse(Buffer<std::vector<Token>>(tokens), diagnostics);
    }

    auto parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
        -> std::vector<Node>;
};

} // namespace xlang
