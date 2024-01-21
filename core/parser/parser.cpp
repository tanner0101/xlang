#include "parser.h"

using namespace xlang;

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> std::optional<Node>;

auto require_next_token(TokenType type, std::string error, Token previousToken,
                        Buffer<std::vector<Token>>& tokens,
                        Diagnostics& diagnostics) -> std::optional<Token> {
    auto token = tokens.safe_pop();
    if (!token.has_value() || token.value().type != type) {
        diagnostics.push_error(error, token.has_value() ? token.value().source
                                                        : previousToken.source);
    }
    return token;
}

auto parse_function_call(Token identifier, Buffer<std::vector<Token>>& tokens,
                         Diagnostics& diagnostics) -> std::optional<Node> {
    auto name = std::get<std::string>(identifier.value);

    auto parenOpen =
        require_next_token(TokenType::paren_open, "Expected open paren",
                           identifier, tokens, diagnostics);
    if (!parenOpen.has_value())
        return std::nullopt;

    std::vector<Node> arguments;
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected function arguments",
                                   parenOpen.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::paren_close) {
            tokens.pop();
            break;
        }

        auto argument = parse_expression(tokens, diagnostics);
        if (!argument.has_value()) {
            diagnostics.push_error("Failed to parse function arguments",
                                   parenOpen.value().source);
            break;
        }
        arguments.push_back(argument.value());
    }

    return Node{FunctionCall{name, arguments, {identifier}}};
}

auto parse_identifier_or_function_call(Token identifier,
                                       Buffer<std::vector<Token>>& tokens,
                                       Diagnostics& diagnostics)
    -> std::optional<Node> {
    auto name = std::get<std::string>(identifier.value);

    auto next = tokens.safe_peek();
    if (next.has_value() && next.value().type == TokenType::paren_open) {
        return parse_function_call(identifier, tokens, diagnostics);
    }

    return Node{NodeType::identifier, name, {identifier}};
}

enum class FunctionDefinitionState {
    none,
    parameters_start,
    parameters,
    body_start,
    body,
};

inline auto functionDefinitionStateToString(FunctionDefinitionState value)
    -> std::string {
#define FUNCTION_DEFINITION_STATE_CASE(name)                                   \
    case FunctionDefinitionState::name:                                        \
        return #name

    switch (value) {
        FUNCTION_DEFINITION_STATE_CASE(none);
        FUNCTION_DEFINITION_STATE_CASE(parameters_start);
        FUNCTION_DEFINITION_STATE_CASE(parameters);
        FUNCTION_DEFINITION_STATE_CASE(body_start);
        FUNCTION_DEFINITION_STATE_CASE(body);
    default:
        return "unknown";
    }
}

inline auto operator<<(std::ostream& os, FunctionDefinitionState value)
    -> std::ostream& {
    return os << functionDefinitionStateToString(value);
}

auto parse_function_definition(Token keyword,
                               Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics)
    -> std::optional<Node> {
    auto identifier =
        require_next_token(TokenType::identifier, "Expected function name",
                           keyword, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    auto name = std::get<std::string>(identifier.value().value);

    auto parenOpen =
        require_next_token(TokenType::paren_open, "Expected open paren",
                           identifier.value(), tokens, diagnostics);
    auto arguments = std::vector<Node>{};
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected function arguments",
                                   identifier.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::paren_close) {
            tokens.pop();
            break;
        }

        auto argument = parse_expression(tokens, diagnostics);
        if (!argument.has_value()) {
            diagnostics.push_error("Expected function argument",
                                   parenOpen.value().source);
            break;
        }
        arguments.push_back(argument.value());
    }

    require_next_token(TokenType::curly_open, "Expected open curly",
                       identifier.value(), tokens, diagnostics);

    auto body = std::vector<Node>{};
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected function body",
                                   identifier.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::curly_close) {
            tokens.pop();
            break;
        }

        auto expr = parse_expression(tokens, diagnostics);
        if (!expr.has_value())
            continue;
        body.push_back(expr.value());
    }

    return std::make_optional(Node{FunctionDefinition{
        name, arguments, body, {keyword, identifier.value()}}});
}

auto parse_variable_definition(Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics, Token varToken)
    -> std::optional<Node> {
    auto identifier =
        require_next_token(TokenType::identifier, "Expected variable name",
                           varToken, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    std::string name = std::get<std::string>(identifier.value().value);

    auto assignment = require_next_token(TokenType::assignment,
                                         "Expected variable assignment",
                                         varToken, tokens, diagnostics);
    if (!assignment.has_value())
        return std::nullopt;

    auto value = parse_expression(tokens, diagnostics);
    if (!value) {
        diagnostics.push_error("Expected variable value",
                               assignment.value().source);
        return std::nullopt;
    }

    return Node{
        VariableDefinition{name,
                           std::make_shared<Node>(value.value()),
                           {varToken, identifier.value(), assignment.value()}}};
}

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> std::optional<Node> {
    switch (tokens.peek().type) {
    case TokenType::identifier:
        return parse_identifier_or_function_call(tokens.pop(), tokens,
                                                 diagnostics);
    case TokenType::function:
        return parse_function_definition(tokens.pop(), tokens, diagnostics);
    case TokenType::variable:
        return parse_variable_definition(tokens, diagnostics, tokens.pop());
    case TokenType::string_literal: {
        auto token = tokens.pop();
        return Node{NodeType::string_literal,
                    std::get<std::string>(token.value),
                    std::vector<Token>{token}};
    } break;
    default:
        return std::nullopt;
    }
}

auto Parser::parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
    -> std::vector<Node> {
    std::vector<Node> expressions{};
    while (!tokens.empty()) {
        auto node = parse_expression(tokens, diagnostics);
        if (node.has_value()) {
            expressions.push_back(node.value());
        } else {
            tokens.pop();
        }
    }
    return expressions;
}
