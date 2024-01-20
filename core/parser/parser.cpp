#include "parser.h"

using namespace xlang;

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> Node;

auto parse_function_call(Buffer<std::vector<Token>>& tokens,
                         Diagnostics& diagnostics) -> Node {
    FunctionCall functionCall{};
    std::vector<Token> consumedTokens{};

    std::cerr << "parse_function_call" << std::endl;

    auto token = tokens.pop();
    consumedTokens.push_back(token);
    if (token.type != TokenType::identifier) {
        diagnostics.push_error("Expected function call identifier",
                               token.source);
        return {functionCall, consumedTokens};
    }
    functionCall.name = std::get<std::string>(token.value);

    if (tokens.empty() || tokens.peek().type != TokenType::paren_open) {
        diagnostics.push_error("Expected function arguments", token.source);
        return {functionCall, consumedTokens};
    }
    tokens.pop();

    while (!tokens.empty()) {
        token = tokens.peek();
        std::cerr << "  " << token << std::endl;
        switch (token.type) {
        case TokenType::string_literal: {
            const auto string = tokens.pop();
            consumedTokens.push_back(string);
            functionCall.arguments.emplace_back(
                NodeType::string_literal, std::get<std::string>(string.value),
                std::vector<Token>{string});
        } break;
        case TokenType::paren_close:
            consumedTokens.push_back(tokens.pop());
            return {functionCall, consumedTokens};
        case TokenType::identifier: {
            const auto identifier = tokens.pop();
            consumedTokens.push_back(identifier);
            functionCall.arguments.emplace_back(
                NodeType::identifier, std::get<std::string>(identifier.value),
                std::vector<Token>{identifier});
        } break;
        default: {
            consumedTokens.push_back(tokens.pop());
            diagnostics.push_error(
                "Unexpected token while parsing function arguments: " +
                    tokenTypeToString(token.type),
                token.source);
        } break;
        }
    }

    return {functionCall, consumedTokens};
}

enum class FunctionDefinitionState {
    none,
    parameters_start,
    parameters,
    body_start,
    body,
};

auto parse_function_definition(Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics) -> Node {
    FunctionDefinition functionDefinition{};
    std::vector<Token> consumedTokens{};

    auto state = FunctionDefinitionState::none;

    std::cerr << "parse_function_definition" << std::endl;

    auto token = tokens.peek();
    if (token.type != TokenType::function) {
        diagnostics.push_error("Expected function keyword", token.source);
    }
    tokens.pop();

    while (!tokens.empty()) {
        token = tokens.peek();
        std::cerr << "  " << token << std::endl;
        switch (state) {
        case FunctionDefinitionState::none: {
            switch (token.type) {
            case TokenType::identifier: {
                auto identifier = tokens.pop();
                consumedTokens.push_back(identifier);
                functionDefinition.name =
                    std::get<std::string>(identifier.value);
                state = FunctionDefinitionState::parameters_start;
            } break;
            default:
                diagnostics.push_error(
                    "Expected function name for function definition",
                    token.source);
                break;
            }
        } break;
        case FunctionDefinitionState::parameters_start: {
            switch (token.type) {
            case TokenType::paren_open:
                consumedTokens.push_back(tokens.pop());
                state = FunctionDefinitionState::parameters;
                break;
            default:
                diagnostics.push_error(
                    "Expected parameter list for function definition",
                    token.source);
                break;
            }
        } break;
        case FunctionDefinitionState::parameters: {
            switch (token.type) {
            case TokenType::identifier: {
                auto identifier = tokens.pop();
                consumedTokens.push_back(identifier);
                functionDefinition.arguments.emplace_back(
                    NodeType::identifier,
                    std::get<std::string>(identifier.value),
                    std::vector<Token>{identifier});
            } break;
            case TokenType::string_literal: {
                auto string = tokens.pop();
                consumedTokens.push_back(string);
                functionDefinition.arguments.emplace_back(
                    NodeType::string_literal,
                    std::get<std::string>(string.value),
                    std::vector<Token>{string});
            } break;
            case TokenType::paren_close: {
                consumedTokens.push_back(tokens.pop());
                state = FunctionDefinitionState::body_start;
            } break;
            default: {
                consumedTokens.push_back(tokens.pop());
                diagnostics.push_error(
                    "Unexpected token while parsing function parameters: " +
                        tokenTypeToString(token.type),
                    token.source);
            } break;
            }
        } break;
        case FunctionDefinitionState::body_start: {
            switch (token.type) {
            case TokenType::curly_open:
                consumedTokens.push_back(tokens.pop());
                state = FunctionDefinitionState::body;
                break;
            default:
                diagnostics.push_error(
                    "Expected function body for function definition",
                    token.source);
                break;
            }
        } break;
        case FunctionDefinitionState::body: {
            std::vector<Node> body{};
            while (token.type != TokenType::curly_close && !tokens.empty()) {
                body.push_back(parse_expression(tokens, diagnostics));
                token = tokens.peek();
            }

            std::cerr << "closing function body" << std::endl;
            functionDefinition.body = body;
            if (tokens.empty()) {
                diagnostics.push_error(
                    "Expected function body to end with curly close",
                    token.source);
            } else {
                consumedTokens.push_back(tokens.pop());
            }
            return {functionDefinition, consumedTokens};
        }
        default:
            break;
        }
    }

    return {functionDefinition, consumedTokens};
}

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> Node {
    std::cerr << "parse_expression: " << tokens.peek() << std::endl;

    switch (tokens.peek().type) {
    case TokenType::identifier:
        return parse_function_call(tokens, diagnostics);
    case TokenType::function:
        return parse_function_definition(tokens, diagnostics);
    default:
        auto token = tokens.pop();
        diagnostics.push_error(
            "Unexpected token: " + tokenTypeToString(token.type), token.source);
        return Node{"unknown", std::vector<Token>{}};
    }
}

auto Parser::parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
    -> std::vector<Node> {
    std::vector<Node> expressions{};
    while (!tokens.empty()) {
        std::cerr << "parse" << std::endl;
        expressions.push_back(parse_expression(tokens, diagnostics));
    }
    return expressions;
}
