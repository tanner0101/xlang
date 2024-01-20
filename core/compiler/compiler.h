#pragma once

#include "../parser/node.h"
#include "../util/buffer.h"
#include "../util/diagnostics.h"
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
