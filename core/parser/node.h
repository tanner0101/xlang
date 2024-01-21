#pragma once

#include "core/util/enum.h"
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "core/lexer/token.h"

namespace xlang {

ENUM_CLASS(NodeType, identifier, variable_definition, function_definition,
           function_call, string_literal);

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

    struct Parameter {
        std::string name;
        std::string type;

        struct Trivia {
            Token identifier;
            Token colon;
            Token type;
        };
        Trivia trivia;
    };
    std::vector<Parameter> parameters;

    std::vector<Node> body;

    struct Trivia {
        Token keyword;
        Token identifier;
    };
    Trivia trivia;
};

inline auto operator==(const FunctionDefinition::Parameter& lhs,
                       const FunctionDefinition::Parameter& rhs) -> bool {
    return lhs.name == rhs.name && lhs.type == rhs.type;
}

inline auto operator==(const FunctionDefinition& lhs,
                       const FunctionDefinition& rhs) -> bool {
    return lhs.name == rhs.name && lhs.parameters == rhs.parameters &&
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

struct Identifier {
    std::string name;

    struct Trivia {
        Token token;
    };
    Trivia trivia;
};

inline auto operator==(const Identifier& lhs, const Identifier& rhs) -> bool {
    return lhs.name == rhs.name;
}

struct StringLiteral {
    std::string value;

    struct Trivia {
        Token token;
    };
    Trivia trivia;
};

inline auto operator==(const StringLiteral& lhs, const StringLiteral& rhs)
    -> bool {
    return lhs.value == rhs.value;
}

using NodeValue = std::variant<StringLiteral, Identifier, FunctionDefinition,
                               FunctionCall, VariableDefinition>;

struct Node {
    NodeType type;
    NodeValue value;

    Node(NodeType type, NodeValue value)
        : type{type}, value{std::move(value)} {}
    Node(FunctionCall functionCall)
        : Node(NodeType::function_call, functionCall) {}
    Node(FunctionDefinition functionDefinition)
        : Node(NodeType::function_definition, functionDefinition) {}
    Node(VariableDefinition variableDefinition)
        : Node(NodeType::variable_definition, variableDefinition) {}
    Node(Identifier identifier) : Node(NodeType::identifier, identifier) {}
    Node(StringLiteral stringLiteral)
        : Node(NodeType::string_literal, stringLiteral) {}
};

inline auto operator<<(std::ostream& os, Node node) -> std::ostream& {
    os << node.type;
    switch (node.type) {
    case NodeType::identifier:
        os << "(" << std::get<Identifier>(node.value).name << ")";
        break;
    case NodeType::string_literal:
        os << "(" << std::get<StringLiteral>(node.value).value << ")";
        break;
    case NodeType::variable_definition: {
        auto variableDefinition = std::get<VariableDefinition>(node.value);
        os << "(" << variableDefinition.name << "=" << variableDefinition.value
           << ")";
    } break;
    case NodeType::function_definition: {
        auto functionDefinition = std::get<FunctionDefinition>(node.value);
        os << "(" << functionDefinition.name << ",";
        for (auto parameter : functionDefinition.parameters) {
            os << parameter.name << ":" << parameter.type;
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
        return std::get<Identifier>(lhs.value) ==
               std::get<Identifier>(rhs.value);
    case NodeType::string_literal:
        return std::get<StringLiteral>(lhs.value) ==
               std::get<StringLiteral>(rhs.value);
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
