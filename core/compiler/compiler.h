#pragma once

#include "core/parser/node.h"
#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include <vector>

namespace xlang {

class Compiler {
  public:
    auto compile(std::vector<Node> ast, Diagnostics& diagnostics)
        -> std::string {
        return compile(Buffer<std::vector<Node>>(ast), diagnostics);
    }

    auto compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
        -> std::string;
};

} // namespace xlang
