#pragma once

#include "core/parser/node.h"
#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include <utility>
#include <vector>

namespace xlang {

auto compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
    -> std::string;

inline auto compile(std::vector<Node> ast, Diagnostics& diagnostics)
    -> std::string {
    return compile(Buffer<std::vector<Node>>(std::move(ast)), diagnostics);
}

} // namespace xlang
