#include <iostream>
#include <string>
#include <variant>
#include <vector>

enum class NodeType {
    identifier,
    function_definition,
    function_call,
    string_literal,
};

std::string nodeTypeToString(NodeType nodeType) {
#define _node_type_case(name)                                                  \
    case NodeType::name:                                                       \
        return #name

    switch (nodeType) {
        _node_type_case(identifier);
        _node_type_case(function_definition);
        _node_type_case(function_call);
        _node_type_case(string_literal);
    default:
        return "unknown";
    }
}

struct ASTNode;

struct FunctionDefinition {
    std::string name;
    std::vector<ASTNode> arguments;
    std::vector<ASTNode> body;
};

bool operator==(const FunctionDefinition &lhs, const FunctionDefinition &rhs) {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments &&
           lhs.body == rhs.body;
}

struct FunctionCall {
    std::string name;
    std::vector<ASTNode> arguments;
};

bool operator==(const FunctionCall &lhs, const FunctionCall &rhs) {
    return lhs.name == rhs.name && lhs.arguments == rhs.arguments;
}

using NodeVariant = std::variant<std::string, FunctionDefinition, FunctionCall>;

struct ASTNode {
    NodeType type;
    NodeVariant content;
};

std::ostream &operator<<(std::ostream &os, ASTNode node) {
    os << nodeTypeToString(node.type);
    switch (node.type) {
    case NodeType::identifier:
    case NodeType::string_literal:
        os << "(" << std::get<std::string>(node.content) << ")";
        break;
    case NodeType::function_definition:
        os << "(" << std::get<FunctionDefinition>(node.content).name << ",";
        for (auto argument :
             std::get<FunctionDefinition>(node.content).arguments) {
            os << argument;
        }
        os << ",";
        for (auto body : std::get<FunctionDefinition>(node.content).body) {
            os << body;
        }
        os << ")";
        break;
    case NodeType::function_call:
        os << "(" << std::get<FunctionCall>(node.content).name << ",";
        for (auto argument : std::get<FunctionCall>(node.content).arguments) {
            os << argument;
        }
        os << ")";
        break;
    default:
        break;
    }
    return os;
}

bool operator==(const ASTNode &lhs, const ASTNode &rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }

    switch (lhs.type) {
    case NodeType::identifier:
    case NodeType::string_literal:
        return std::get<std::string>(lhs.content) ==
               std::get<std::string>(rhs.content);
    case NodeType::function_definition:
        return std::get<FunctionDefinition>(lhs.content) ==
               std::get<FunctionDefinition>(rhs.content);
    case NodeType::function_call:
        return std::get<FunctionCall>(lhs.content) ==
               std::get<FunctionCall>(rhs.content);
    default:
        return false;
    }

    return false;
}