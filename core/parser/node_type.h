#pragma once

#include <string>

namespace xlang {

enum class NodeType {
    identifier,
    variable_definition,
    function_definition,
    function_call,
    string_literal,
};

inline auto nodeTypeToString(NodeType nodeType) -> std::string {
#define NODE_TYPE_CASE(name)                                                   \
    case NodeType::name:                                                       \
        return #name

    switch (nodeType) {
        NODE_TYPE_CASE(identifier);
        NODE_TYPE_CASE(variable_definition);
        NODE_TYPE_CASE(function_definition);
        NODE_TYPE_CASE(function_call);
        NODE_TYPE_CASE(string_literal);
    default:
        return "unknown";
    }
}

} // namespace xlang
