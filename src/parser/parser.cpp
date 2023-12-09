#include "parser.h"
#include <cassert>

auto parse_expression(Buffer<std::vector<Token>>& tokens) -> std::vector<Node>;

auto parse_function_call(Buffer<std::vector<Token>>& tokens) -> FunctionCall {
    FunctionCall functionCall{};

    assert(tokens.peek().type == TokenType::identifier);
    functionCall.name = std::get<std::string>(tokens.pop().value);
    assert(tokens.pop().type == TokenType::paren_open);

    while (!tokens.empty()) {
        switch (tokens.peek().type) {
        case TokenType::string_literal: {
            const auto string = tokens.pop();
            functionCall.arguments.emplace_back(
                NodeType::string_literal, std::get<std::string>(string.value));
        } break;
        case TokenType::paren_close:
            tokens.pop();
            return functionCall;
        default:
            assert(false);
            break;
        }
    }

    return functionCall;
}

enum class FunctionDefinitionState {
    none,
    parameters_start,
    parameters,
    body_start,
    body,
};

auto parse_function_definition(Buffer<std::vector<Token>>& tokens)
    -> FunctionDefinition {
    FunctionDefinition functionDefinition{};

    auto state = FunctionDefinitionState::none;

    assert(tokens.pop().type == TokenType::function);

    while (!tokens.empty()) {
        switch (state) {
        case FunctionDefinitionState::none: {
            switch (tokens.peek().type) {
            case TokenType::identifier: {
                auto identifier = tokens.pop();
                functionDefinition.name =
                    std::get<std::string>(identifier.value);
                state = FunctionDefinitionState::parameters_start;
            } break;
            default:
                assert(false);
                break;
            }
        } break;
        case FunctionDefinitionState::parameters_start: {
            switch (tokens.peek().type) {
            case TokenType::paren_open:
                tokens.pop();
                state = FunctionDefinitionState::parameters;
                break;
            default:
                assert(false);
                break;
            }
        } break;
        case FunctionDefinitionState::parameters: {
            switch (tokens.peek().type) {
            case TokenType::identifier: {
                auto identifier = tokens.pop();
                functionDefinition.arguments.emplace_back(
                    NodeType::identifier,
                    std::get<std::string>(identifier.value));
            } break;
            case TokenType::string_literal: {
                auto string = tokens.pop();
                functionDefinition.arguments.emplace_back(
                    NodeType::string_literal,
                    std::get<std::string>(string.value));
            } break;
            case TokenType::paren_close: {
                tokens.pop();
                state = FunctionDefinitionState::body_start;
            } break;
            default: {
                assert(false);
            } break;
            }
        } break;
        case FunctionDefinitionState::body_start: {
            switch (tokens.peek().type) {
            case TokenType::curly_open:
                tokens.pop();
                state = FunctionDefinitionState::body;
                break;
            default:
                assert(false);
                break;
            }
        } break;
        case FunctionDefinitionState::body: {
            functionDefinition.body = parse_expression(tokens);
            auto body = tokens.pop();
            assert(body.type == TokenType::curly_close);
            return functionDefinition;
        }
        default:
            break;
        }
    }

    return functionDefinition;
}
auto parse_expression(Buffer<std::vector<Token>>& tokens) -> std::vector<Node> {
    std::vector<Node> expression{};

    while (!tokens.empty()) {
        switch (tokens.peek().type) {
        case TokenType::identifier:
            expression.emplace_back(parse_function_call(tokens));
            break;
        case TokenType::function:
            expression.emplace_back(parse_function_definition(tokens));
            break;
        default:
            return expression;
        }
    }

    return expression;
}

auto Parser::parse(Buffer<std::vector<Token>> tokens) -> std::vector<Node> {
    return parse_expression(tokens);
}
