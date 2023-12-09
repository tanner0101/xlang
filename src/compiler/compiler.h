#pragma once

#include "../parser/node.h"
#include "../util/buffer.h"
#include <vector>

namespace xlang {

class Compiler {
  public:
    auto compile(std::vector<Node> ast) -> std::string {
        return compile(Buffer<std::vector<Node>>(ast));
    }

    auto compile(Buffer<std::vector<Node>> ast) -> std::string;
};

} // namespace xlang
