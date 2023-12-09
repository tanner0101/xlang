#pragma once

#include <string>
#include <vector>

struct Node;

struct FunctionDefinition {
    std::string name;
    std::vector<Node> arguments;
    std::vector<Node> body;
};

inline auto operator==(const FunctionDefinition& lhs,
                       const FunctionDefinition& rhs) -> bool {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments &&
           lhs.body == rhs.body;
}
