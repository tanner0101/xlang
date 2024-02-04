#pragma once

#include "core/util/enum.h"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/lexer/token.h"

namespace xlang {

ENUM_CLASS(NodeType, identifier, variable_definition, function_definition,
           function_call, member_access, string_literal, integer_literal,
           struct_definition);

struct Node;

struct TypeIdentifier {
    std::string name;

    std::vector<TypeIdentifier> genericParameters;

    struct Tokens {
        Token name;
        std::optional<Token> genericOpen;
        std::optional<Token> genericClose;

        auto operator==(const Tokens& other) const -> bool = default;
    };

    Tokens tokens;

    auto operator==(const TypeIdentifier& other) const -> bool = default;
};

inline auto operator<<(std::ostream& os, const TypeIdentifier& value)
    -> std::ostream& {
    os << value.name;
    if (!value.genericParameters.empty()) {
        os << "<";
    }
    bool first = true;
    for (const auto& generic_parameter : value.genericParameters) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << generic_parameter;
    }
    if (!value.genericParameters.empty()) {
        os << ">";
    }
    return os;
}

struct StructDefinition {
    std::string name;

    struct Member {
        std::string name;
        TypeIdentifier type;

        struct Tokens {
            Token name;
            Token colon;
            auto operator==(const Tokens& other) const -> bool = default;
        };

        Tokens tokens;

        auto operator==(const Member& other) const -> bool = default;
    };

    std::vector<Member> members;

    struct Tokens {
        Token keyword;
        Token identifier;
        Token curlyOpen;
        Token curlyClose;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const StructDefinition& other) const -> bool = default;
};

struct FunctionCall {
    std::string name;
    std::vector<Node> arguments;

    struct Tokens {
        Token identifier;
        Token parenOpen;
        Token parenClose;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const FunctionCall& other) const -> bool = default;
};

struct MemberAccess {
    std::shared_ptr<Node> base;
    std::shared_ptr<Node> member;

    struct Tokens {
        Token dot;
        auto operator==(const Tokens& other) const -> bool = default;
    };
    Tokens tokens;

    auto operator==(const MemberAccess& other) const -> bool = default;
};

struct FunctionDefinition {
    std::string name;
    bool external;

    struct Parameter {
        std::string name;
        TypeIdentifier type;

        struct Tokens {
            Token identifier;
            Token colon;
            auto operator==(const Tokens& other) const -> bool = default;
        };
        Tokens tokens;

        auto operator==(const Parameter& other) const -> bool = default;
    };
    std::vector<Parameter> parameters;

    std::vector<Node> body;

    struct Tokens {
        std::optional<Token> external;
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

using NodeValue = std::variant<StringLiteral, IntegerLiteral, Identifier,
                               FunctionDefinition, FunctionCall, MemberAccess,
                               VariableDefinition, StructDefinition>;

struct Node {
    NodeType type;
    NodeValue value;

    Node(NodeType type, NodeValue value)
        : type{type}, value{std::move(value)} {}
    Node(FunctionCall value) : Node(NodeType::function_call, value) {}
    Node(MemberAccess value) : Node(NodeType::member_access, value) {}
    Node(FunctionDefinition value)
        : Node(NodeType::function_definition, value) {}
    Node(VariableDefinition value)
        : Node(NodeType::variable_definition, value) {}
    Node(StructDefinition value) : Node(NodeType::struct_definition, value) {}
    Node(Identifier identifier) : Node(NodeType::identifier, identifier) {}
    Node(StringLiteral stringLiteral)
        : Node(NodeType::string_literal, stringLiteral) {}
    Node(IntegerLiteral integerLiteral)
        : Node(NodeType::integer_literal, integerLiteral) {}
};

inline auto node_source(Node node) -> Source {
    switch (node.type) {
    case NodeType::identifier:
        return std::get<Identifier>(node.value).token.source;
    case NodeType::integer_literal:
        return std::get<IntegerLiteral>(node.value).token.source;
    case NodeType::string_literal:
        return std::get<StringLiteral>(node.value).token.source;
    default:
        return Source{};
    }
}

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
        auto variable_definition = std::get<VariableDefinition>(node.value);
        os << "(" << variable_definition.name << "="
           << *variable_definition.value << ")";
    } break;
    case NodeType::struct_definition: {
        auto value = std::get<StructDefinition>(node.value);
        os << "(" << value.name;
        if (!value.members.empty()) {
            os << ",";
        }
        for (const auto& member : value.members) {
            os << member.name << ":" << member.type;
        }
        os << ")";
    } break;
    case NodeType::function_definition: {
        auto value = std::get<FunctionDefinition>(node.value);
        os << "(" << value.name;
        if (value.external) {
            os << ",external";
        }
        if (!value.parameters.empty()) {
            os << ",";
        }
        auto first = true;
        for (const auto& parameter : value.parameters) {
            if (!first) {
                os << ",";
            }
            first = false;
            os << parameter.name << ":" << parameter.type;
        }
        if (!value.body.empty()) {
            os << ",";
        }
        first = true;
        for (const auto& body : value.body) {
            if (!first) {
                os << ",";
            }
            first = false;
            os << body;
        }
        os << ")";
    } break;
    case NodeType::function_call: {
        auto function_call = std::get<FunctionCall>(node.value);
        os << "(" << function_call.name << ",";
        for (const auto& argument : function_call.arguments) {
            os << argument;
        }
        os << ")";
    } break;
    case NodeType::member_access: {
        auto value = std::get<MemberAccess>(node.value);
        os << "(" << *value.base << " > " << *value.member << ")";
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
