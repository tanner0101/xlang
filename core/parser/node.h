#pragma once

#include "node_type.h"
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "core/lexer/token.h"

namespace xlang {

struct Node;

struct FunctionCall {
    std::string name;
    std::vector<Node> arguments;

    struct Trivia {
        Token identifier;
    };
    Trivia trivia;
};

inline auto operator==(const FunctionCall& lhs, const FunctionCall& rhs)
    -> bool {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments;
}

struct FunctionDefinition {
    std::string name;
    std::vector<Node> arguments;
    std::vector<Node> body;

    struct Trivia {
        Token keyword;
        Token identifier;
    };
    Trivia trivia;
};

inline auto operator==(const FunctionDefinition& lhs,
                       const FunctionDefinition& rhs) -> bool {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments &&
           lhs.body == rhs.body;
}

struct VariableDefinition {
    std::string name;
    std::shared_ptr<Node> value;

    struct Trivia {
        Token keyword;
        Token identifier;
        Token assignment;
    };
    Trivia trivia;
};

inline auto operator==(const VariableDefinition& lhs,
                       const VariableDefinition& rhs) -> bool {
    return lhs.name == rhs.name && lhs.value == rhs.value;
}

using NodeValue = std::variant<std::string, FunctionDefinition, FunctionCall,
                               VariableDefinition>;

struct Node {
    NodeType type;
    NodeValue value;
    std::vector<Token> tokens;

    Node(NodeType type, NodeValue value, std::vector<Token> tokens)
        : type{type}, value{std::move(value)}, tokens{std::move(tokens)} {}
    Node(FunctionCall functionCall)
        : Node(NodeType::function_call, functionCall, {}) {}
    Node(FunctionDefinition functionDefinition)
        : Node(NodeType::function_definition, functionDefinition, {}) {}
    Node(VariableDefinition variableDefinition)
        : Node(NodeType::variable_definition, variableDefinition, {}) {}
    Node(const char* string, std::vector<Token> tokens)
        : Node(NodeType::string_literal, std::string(string), tokens) {}
};

inline auto operator<<(std::ostream& os, Node node) -> std::ostream& {
    os << nodeTypeToString(node.type);
    switch (node.type) {
    case NodeType::identifier:
    case NodeType::string_literal:
        os << "(" << std::get<std::string>(node.value) << ")";
        break;
    case NodeType::variable_definition: {
        auto variableDefinition = std::get<VariableDefinition>(node.value);
        os << "(" << variableDefinition.name << "=" << variableDefinition.value
           << ")";
    } break;
    case NodeType::function_definition: {
        auto functionDefinition = std::get<FunctionDefinition>(node.value);
        os << "(" << functionDefinition.name << ",";
        for (auto argument : functionDefinition.arguments) {
            os << argument;
        }
        os << ",";
        for (auto body : functionDefinition.body) {
            os << body;
        }
        os << ")";
    } break;
    case NodeType::function_call: {
        auto functionCall = std::get<FunctionCall>(node.value);
        os << "(" << functionCall.name << ",";
        for (auto argument : functionCall.arguments) {
            os << argument;
        }
        os << ")";
    } break;
    default:
        break;
    }
    return os;
}

inline auto operator==(const Node& lhs, const Node& rhs) -> bool {
    if (lhs.type != rhs.type) {
        return false;
    }

    switch (lhs.type) {
    case NodeType::identifier:
    case NodeType::string_literal:
        return std::get<std::string>(lhs.value) ==
               std::get<std::string>(rhs.value);
    case NodeType::function_definition:
        return std::get<FunctionDefinition>(lhs.value) ==
               std::get<FunctionDefinition>(rhs.value);
    case NodeType::function_call:
        return std::get<FunctionCall>(lhs.value) ==
               std::get<FunctionCall>(rhs.value);
    default:
        return false;
    }

    return false;
}

} // namespace xlang
