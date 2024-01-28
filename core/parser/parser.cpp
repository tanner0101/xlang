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
        return std::nullopt;
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

auto parse_identifier_or_function_call(Token previousToken,
                                       Buffer<std::vector<Token>>& tokens,
                                       Diagnostics& diagnostics)
    -> std::optional<Node> {

    auto identifier =
        require_next_token(TokenType::identifier, "Expected identifier",
                           previousToken, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    auto name = std::get<std::string>(identifier.value().value);

    std::shared_ptr<Identifier> nested = nullptr;
    auto next = tokens.safe_peek();
    if (next.has_value()) {
        if (next.value().type == TokenType::paren_open) {
            return parse_function_call(identifier.value(), tokens, diagnostics);
        } else if (next.value().type == TokenType::dot) {
            auto maybeNested = parse_identifier_or_function_call(
                tokens.pop(), tokens, diagnostics);
            if (!maybeNested.has_value() ||
                maybeNested.value().type != NodeType::identifier) {
                diagnostics.push_error("Expected nested identifier",
                                       identifier.value().source);
                return std::nullopt;
            }
            nested = std::make_shared<Identifier>(
                std::get<Identifier>(maybeNested.value().value));
        }
    }

    return Node{Identifier{name, nested, {identifier.value()}}};
}

auto parse_type_identifier(Token previousToken,
                           Buffer<std::vector<Token>>& tokens,
                           Diagnostics& diagnostics)
    -> std::optional<TypeIdentifier> {
    auto name = require_next_token(TokenType::identifier, "Expected type name",
                                   previousToken, tokens, diagnostics);

    if (!name.has_value())
        return std::nullopt;

    auto genericParameters = std::vector<TypeIdentifier>{};

    auto maybeGenericStart = tokens.safe_peek();
    std::optional<Token> maybeGenericEnd = std::nullopt;
    if (maybeGenericStart.has_value() &&
        maybeGenericStart.value().type == TokenType::angle_open) {

        auto genericParameter =
            parse_type_identifier(tokens.pop(), tokens, diagnostics);
        if (!genericParameter.has_value())
            return std::nullopt;

        genericParameters.push_back(genericParameter.value());

        // TODO: support more than one generic parameter

        maybeGenericEnd = require_next_token(TokenType::angle_close,
                                             "Expected close angle bracket",
                                             name.value(), tokens, diagnostics);
        if (!maybeGenericEnd.has_value())
            return std::nullopt;
    }

    return std::make_optional(
        TypeIdentifier{std::get<std::string>(name.value().value),
                       genericParameters,
                       {name.value(), maybeGenericStart, maybeGenericEnd}});
}

auto parse_function_definition_parameter(Token keyword,
                                         Buffer<std::vector<Token>>& tokens,
                                         Diagnostics& diagnostics,
                                         Token previous)
    -> std::optional<FunctionDefinition::Parameter> {

    auto identifier =
        require_next_token(TokenType::identifier, "Expected argument name",
                           previous, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    auto colon = require_next_token(TokenType::colon, "Expected colon",
                                    identifier.value(), tokens, diagnostics);
    if (!colon.has_value())
        return std::nullopt;

    auto type = parse_type_identifier(colon.value(), tokens, diagnostics);
    if (!type.has_value()) {
        return std::nullopt;
    }

    return std::make_optional(FunctionDefinition::Parameter{
        std::get<std::string>(identifier.value().value),
        type.value(),
        {identifier.value(), colon.value()}});
}

auto parse_struct_member(Token keyword, Buffer<std::vector<Token>>& tokens,
                         Diagnostics& diagnostics)
    -> std::optional<StructDefinition::Member> {
    auto identifier =
        require_next_token(TokenType::identifier, "Expected member name",
                           keyword, tokens, diagnostics);
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    auto colon = require_next_token(TokenType::colon, "Expected colon", keyword,
                                    tokens, diagnostics);
    if (!colon.has_value()) {
        return std::nullopt;
    }

    auto type = parse_type_identifier(colon.value(), tokens, diagnostics);
    if (!type.has_value()) {
        return std::nullopt;
    }

    return std::make_optional(StructDefinition::Member{
        std::get<std::string>(identifier.value().value),
        type.value(),
        {identifier.value(), colon.value()},
    });
}

auto parse_struct_definition(Token keyword, Buffer<std::vector<Token>>& tokens,
                             Diagnostics& diagnostics) -> std::optional<Node> {
    auto identifier =
        require_next_token(TokenType::identifier, "Expected struct name",
                           keyword, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    auto name = std::get<std::string>(identifier.value().value);

    auto curlyOpen =
        require_next_token(TokenType::curly_open, "Expected open curly",
                           identifier.value(), tokens, diagnostics);
    if (!curlyOpen.has_value())
        return std::nullopt;

    auto members = std::vector<StructDefinition::Member>{};

    std::optional<Token> curlyClose = std::nullopt;
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected struct members",
                                   identifier.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::curly_close) {
            curlyClose = tokens.safe_pop();
            break;
        }

        auto member = parse_struct_member(token.value(), tokens, diagnostics);
        if (!member.has_value()) {
            tokens.pop();
            continue;
        }
        members.push_back(member.value());
    }

    if (!curlyClose.has_value())
        return std::nullopt;

    return std::make_optional(Node{StructDefinition{
        name,
        members,
        {keyword, identifier.value(), curlyOpen.value(), curlyClose.value()}}});
}

auto parse_function_definition(Token keyword,
                               Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics)
    -> std::optional<Node> {
    std::optional<Token> externalKeyword;
    if (keyword.type == TokenType::external) {
        externalKeyword = keyword;
        auto maybeKeyword =
            require_next_token(TokenType::function, "Expected function keyword",
                               keyword, tokens, diagnostics);
        if (!maybeKeyword.has_value())
            return std::nullopt;
        keyword = maybeKeyword.value();
    }

    auto identifier =
        require_next_token(TokenType::identifier, "Expected function name",
                           keyword, tokens, diagnostics);
    if (!identifier.has_value())
        return std::nullopt;

    auto name = std::get<std::string>(identifier.value().value);

    auto parenOpen =
        require_next_token(TokenType::paren_open, "Expected open paren",
                           identifier.value(), tokens, diagnostics);
    auto parameters = std::vector<FunctionDefinition::Parameter>{};
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

        if (token.value().type == TokenType::comma) {
            tokens.pop();
        }

        auto parameter = parse_function_definition_parameter(
            keyword, tokens, diagnostics, identifier.value());
        if (!parameter.has_value())
            continue;

        parameters.push_back(parameter.value());
    }

    auto body = std::vector<Node>{};
    if (!externalKeyword.has_value()) {
        require_next_token(TokenType::curly_open, "Expected open curly",
                           identifier.value(), tokens, diagnostics);

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
            if (!expr.has_value()) {
                tokens.pop();
                continue;
            }
            body.push_back(expr.value());
        }
    }

    return std::make_optional(Node{
        FunctionDefinition{name,
                           externalKeyword.has_value(),
                           parameters,
                           body,
                           {externalKeyword, keyword, identifier.value()}}});
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

    auto assignment =
        require_next_token(TokenType::equal, "Expected variable assignment",
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
        return parse_identifier_or_function_call(tokens.peek(), tokens,
                                                 diagnostics);
    case TokenType::structure:
        return parse_struct_definition(tokens.pop(), tokens, diagnostics);
    case TokenType::function:
    case TokenType::external:
        return parse_function_definition(tokens.pop(), tokens, diagnostics);
    case TokenType::variable:
        return parse_variable_definition(tokens, diagnostics, tokens.pop());
    case TokenType::string_literal: {
        auto token = tokens.pop();
        return Node{StringLiteral{std::get<std::string>(token.value), {token}}};
    } break;
    case TokenType::integer_literal: {
        auto token = tokens.pop();
        return Node{IntegerLiteral{stoull(std::get<std::string>(token.value)),
                                   {token}}};
    } break;
    default:
        std::cerr << "Unexpected token: " << tokens.peek().type << std::endl;
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
