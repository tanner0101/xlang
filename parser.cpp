#include "parser.h"
#include <cassert>

Parser::Parser() {}

Parser::~Parser() {}

FunctionCall parse_function_call(Buffer<std::vector<Token>> &tokens) {
    FunctionCall functionCall{};

    auto token = tokens.pop().value();
    assert(token.type == TokenType::identifier);
    functionCall.name = token.value.value();

    switch (tokens.peek().value().type) {
    case TokenType::paren_open:
        tokens.pop();
        break;
    default:
        assert(false);
        break;
    }

    while (tokens.peek().has_value()) {
        switch (tokens.peek().value().type) {
        case TokenType::string_literal:
            token = tokens.pop().value();
            functionCall.arguments.push_back(
                ASTNode{NodeType::string_literal, token.value.value()});
            break;
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

std::vector<ASTNode> parse_expression(Buffer<std::vector<Token>> &tokens) {
    std::vector<ASTNode> expression{};

    while (tokens.peek().has_value()) {
        switch (tokens.peek().value().type) {
        case TokenType::identifier:
            expression.push_back(
                ASTNode{NodeType::function_call, parse_function_call(tokens)});
            break;
        default:
            return expression;
        }
    }

    return expression;
}

enum class FunctionDefinitionState {
    none,
    parameters_start,
    parameters,
    body_start,
    body,
};

FunctionDefinition
parse_function_definition(Buffer<std::vector<Token>> &tokens) {
    FunctionDefinition functionDefinition{};

    auto state = FunctionDefinitionState::none;
    Token token;

    while (tokens.peek().has_value()) {
        switch (state) {
        case FunctionDefinitionState::none:
            switch (tokens.peek().value().type) {
            case TokenType::identifier:
                token = tokens.pop().value();
                functionDefinition.name = token.value.value();
                state = FunctionDefinitionState::parameters_start;
                break;
            default:
                assert(false);
                break;
            }
            break;
        case FunctionDefinitionState::parameters_start:
            switch (tokens.peek().value().type) {
            case TokenType::paren_open:
                tokens.pop();
                state = FunctionDefinitionState::parameters;
                break;
            default:
                assert(false);
                break;
            }
            break;
        case FunctionDefinitionState::parameters:
            switch (tokens.peek().value().type) {
            case TokenType::identifier:
                token = tokens.pop().value();
                functionDefinition.arguments.push_back(
                    ASTNode{NodeType::identifier, token.value.value()});
            case TokenType::string_literal:
                functionDefinition.arguments.push_back(
                    ASTNode{NodeType::string_literal, token.value.value()});
                break;
            case TokenType::paren_close:
                tokens.pop();
                state = FunctionDefinitionState::body_start;
                break;
            default:
                assert(false);
                break;
            }
            break;
        case FunctionDefinitionState::body_start:
            switch (tokens.peek().value().type) {
            case TokenType::curly_open:
                tokens.pop();
                state = FunctionDefinitionState::body;
                break;
            default:
                assert(false);
                break;
            }
            break;
        case FunctionDefinitionState::body:
            functionDefinition.body = parse_expression(tokens);
            token = tokens.pop().value();
            assert(token.type == TokenType::curly_close);
            return functionDefinition;
        default:
            break;
        }
    }

    return functionDefinition;
}

enum class ParserState {
    none,
    functionDeclaration,
};

std::vector<ASTNode> Parser::parse(Buffer<std::vector<Token>> tokens) {
    auto state = ParserState::none;

    std::vector<ASTNode> ast{};

    while (tokens.peek().has_value()) {
        switch (state) {
        case ParserState::none:
            switch (tokens.peek().value().type) {
            case TokenType::function:
                state = ParserState::functionDeclaration;
                tokens.pop();
                break;
            default:
                break;
            }
            break;
        case ParserState::functionDeclaration:
            ast.push_back(ASTNode{NodeType::function_definition,
                                  parse_function_definition(tokens)});
            state = ParserState::none;
            break;
        }
    }

    return ast;
}