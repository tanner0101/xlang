#pragma once

#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include "token.h"
#include <string>
#include <utility>
#include <vector>

namespace xlang {

auto lex(Buffer<std::string> input, Diagnostics& diagnostics)
    -> std::vector<Token>;

inline auto lex(std::string input, Diagnostics& diagnostics)
    -> std::vector<Token> {
    return lex(Buffer<std::string>(std::move(input)), diagnostics);
}

} // namespace xlang
