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
           function_call, string_literal, integer_literal);

struct Node;

struct FunctionCall {
    std::string name;
    std::vector<Node> arguments;

    struct Tokens {
        Token identifier;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const FunctionCall& other) const -> bool = default;
};

struct FunctionDefinition {
    std::string name;

    struct Parameter {
        std::string name;
        std::string type;

        struct Tokens {
            Token identifier;
            Token colon;
            Token type;
            auto operator==(const Tokens& other) const -> bool = default;
        };
        Tokens tokens;

        auto operator==(const Parameter& other) const -> bool = default;
    };
    std::vector<Parameter> parameters;

    std::vector<Node> body;

    struct Tokens {
        Token keyword;
        Token identifier;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const FunctionDefinition& other) const -> bool = default;
};

struct VariableDefinition {
    std::string name;
    std::shared_ptr<Node> value;

    struct Tokens {
        Token keyword;
        Token identifier;
        Token assignment;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const VariableDefinition& other) const -> bool = default;
};

struct Identifier {
    std::string name;
    Token token;
    auto operator==(const Identifier& other) const -> bool = default;
};

struct StringLiteral {
    std::string value;
    Token token;
    auto operator==(const StringLiteral& other) const -> bool = default;
};

struct IntegerLiteral {
    uint64_t value;
    Token token;
    auto operator==(const IntegerLiteral& other) const -> bool = default;
};

using NodeValue =
    std::variant<StringLiteral, IntegerLiteral, Identifier, FunctionDefinition,
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
    Node(IntegerLiteral integerLiteral)
        : Node(NodeType::integer_literal, integerLiteral) {}
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
