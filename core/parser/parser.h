#pragma once

#include "core/lexer/token.h"
#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include "node.h"
#include <vector>

namespace xlang {

auto parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
    -> std::vector<Node>;

inline auto parse(std::vector<Token> tokens, Diagnostics& diagnostics)
    -> std::vector<Node> {
    return parse(Buffer<std::vector<Token>>(std::move(tokens)), diagnostics);
}

} // namespace xlang
