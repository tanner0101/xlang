#pragma once

#include <string>
#include <vector>

struct Node;

struct FunctionCall {
    std::string name;
    std::vector<Node> arguments;
};

inline auto operator==(const FunctionCall& lhs, const FunctionCall& rhs)
    -> bool {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments;
}