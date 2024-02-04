#include "parser.h"
#include "core/lexer/token.h"

using namespace xlang;

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> std::optional<Node>;

auto require_next_token(TokenType type, const std::string& error,
                        const Token& previousToken,
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

auto parse_function_call(const Token& identifier,
                         Buffer<std::vector<Token>>& tokens,
                         Diagnostics& diagnostics) -> std::optional<Node> {
    auto name = std::get<std::string>(identifier.value);

    auto paren_open =
        require_next_token(TokenType::paren_open, "Expected open paren",
                           identifier, tokens, diagnostics);
    if (!paren_open.has_value()) {
        return std::nullopt;
    }

    std::vector<Node> arguments;
    std::optional<Token> paren_close = std::nullopt;
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected function arguments",
                                   paren_open.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::paren_close) {
            paren_close = tokens.safe_pop();
            break;
        }

        auto argument = parse_expression(tokens, diagnostics);
        if (!argument.has_value()) {
            diagnostics.push_error("Failed to parse function arguments",
                                   paren_open.value().source);
            break;
        }
        arguments.push_back(argument.value());
    }

    if (!paren_close.has_value()) {
        return std::nullopt;
    }

    return Node{
        FunctionCall{name,
                     arguments,
                     {identifier, paren_open.value(), paren_close.value()}}};
}

auto parse_identifier_or_function_call(const Token& previousToken,
                                       Buffer<std::vector<Token>>& tokens,
                                       Diagnostics& diagnostics)
    -> std::optional<Node> {

    auto identifier =
        require_next_token(TokenType::identifier, "Expected identifier",
                           previousToken, tokens, diagnostics);
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    auto name = std::get<std::string>(identifier.value().value);

    auto next = tokens.safe_peek();
    if (next.has_value() && next.value().type == TokenType::paren_open) {
        return parse_function_call(identifier.value(), tokens, diagnostics);
    }

    return Node{Identifier{name, {identifier.value()}}};
}

auto parse_type_identifier(const Token& previousToken,
                           Buffer<std::vector<Token>>& tokens,
                           Diagnostics& diagnostics)
    -> std::optional<TypeIdentifier> {
    auto name = require_next_token(TokenType::identifier, "Expected type name",
                                   previousToken, tokens, diagnostics);

    if (!name.has_value()) {
        return std::nullopt;
    }

    auto generic_parameters = std::vector<TypeIdentifier>{};

    auto maybe_generic_start = tokens.safe_peek();
    std::optional<Token> maybe_generic_end = std::nullopt;
    if (maybe_generic_start.has_value() &&
        maybe_generic_start.value().type == TokenType::angle_open) {

        auto generic_parameter =
            parse_type_identifier(tokens.pop(), tokens, diagnostics);
        if (!generic_parameter.has_value()) {
            return std::nullopt;
        }

        generic_parameters.push_back(generic_parameter.value());

        // TODO: support more than one generic parameter

        maybe_generic_end = require_next_token(
            TokenType::angle_close, "Expected close angle bracket",
            name.value(), tokens, diagnostics);
        if (!maybe_generic_end.has_value()) {
            return std::nullopt;
        }
    }

    return std::make_optional(
        TypeIdentifier{std::get<std::string>(name.value().value),
                       generic_parameters,
                       {name.value(), maybe_generic_start, maybe_generic_end}});
}

auto parse_function_definition_parameter(const Token& keyword,
                                         Buffer<std::vector<Token>>& tokens,
                                         Diagnostics& diagnostics,
                                         const Token& previous)
    -> std::optional<FunctionDefinition::Parameter> {

    auto identifier =
        require_next_token(TokenType::identifier, "Expected argument name",
                           previous, tokens, diagnostics);
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    auto colon = require_next_token(TokenType::colon, "Expected colon",
                                    identifier.value(), tokens, diagnostics);
    if (!colon.has_value()) {
        return std::nullopt;
    }

    auto type = parse_type_identifier(colon.value(), tokens, diagnostics);
    if (!type.has_value()) {
        return std::nullopt;
    }

    return std::make_optional(FunctionDefinition::Parameter{
        std::get<std::string>(identifier.value().value),
        type.value(),
        {identifier.value(), colon.value()}});
}

auto parse_struct_member(const Token& keyword,
                         Buffer<std::vector<Token>>& tokens,
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
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    auto name = std::get<std::string>(identifier.value().value);

    auto curly_open = require_next_token(
        TokenType::curly_open,
        "Expected open curly while parsing struct definition",
        identifier.value(), tokens, diagnostics);
    if (!curly_open.has_value()) {
        return std::nullopt;
    }

    auto members = std::vector<StructDefinition::Member>{};

    std::optional<Token> curly_close = std::nullopt;
    while (true) {
        auto token = tokens.safe_peek();
        if (!token.has_value()) {
            diagnostics.push_error("Expected struct members",
                                   identifier.value().source);
            return std::nullopt;
        }

        if (token.value().type == TokenType::curly_close) {
            curly_close = tokens.safe_pop();
            break;
        }

        auto member = parse_struct_member(token.value(), tokens, diagnostics);
        if (!member.has_value()) {
            tokens.pop();
            continue;
        }
        members.push_back(member.value());
    }

    if (!curly_close.has_value()) {
        return std::nullopt;
    }

    return std::make_optional(
        Node{StructDefinition{name,
                              members,
                              {keyword, identifier.value(), curly_open.value(),
                               curly_close.value()}}});
}

auto parse_function_definition(Token keyword,
                               Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics)
    -> std::optional<Node> {
    std::optional<Token> external_keyword;
    if (keyword.type == TokenType::external) {
        external_keyword = keyword;
        auto maybe_keyword =
            require_next_token(TokenType::function, "Expected function keyword",
                               keyword, tokens, diagnostics);
        if (!maybe_keyword.has_value()) {
            return std::nullopt;
        }
        keyword = maybe_keyword.value();
    }

    auto identifier =
        require_next_token(TokenType::identifier, "Expected function name",
                           keyword, tokens, diagnostics);
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    auto name = std::get<std::string>(identifier.value().value);

    auto paren_open =
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
        if (!parameter.has_value()) {
            continue;
        }

        parameters.push_back(parameter.value());
    }

    auto body = std::vector<Node>{};
    if (!external_keyword.has_value()) {
        require_next_token(TokenType::curly_open,
                           "Expected open curly parsing function definition",
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
                tokens.safe_pop();
                continue;
            }
            body.push_back(expr.value());
        }
    }

    return std::make_optional(Node{
        FunctionDefinition{name,
                           external_keyword.has_value(),
                           parameters,
                           body,
                           {external_keyword, keyword, identifier.value()}}});
}

auto parse_variable_definition(Buffer<std::vector<Token>>& tokens,
                               Diagnostics& diagnostics, Token varToken)
    -> std::optional<Node> {
    auto identifier =
        require_next_token(TokenType::identifier, "Expected variable name",
                           varToken, tokens, diagnostics);
    if (!identifier.has_value()) {
        return std::nullopt;
    }

    std::string name = std::get<std::string>(identifier.value().value);

    auto assignment =
        require_next_token(TokenType::equal, "Expected variable assignment",
                           varToken, tokens, diagnostics);
    if (!assignment.has_value()) {
        return std::nullopt;
    }

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

auto peek_token_type(Buffer<std::vector<Token>>& tokens, TokenType type)
    -> bool {
    if (tokens.empty()) {
        return false;
    }

    return tokens.peek().type == type;
}

auto parse_expression(Buffer<std::vector<Token>>& tokens,
                      Diagnostics& diagnostics) -> std::optional<Node> {
    std::cerr << "Parsing expression " << tokens.peek() << '\n';
    std::optional<Node> value;
    switch (tokens.peek().type) {
    case TokenType::identifier: {
        value = parse_identifier_or_function_call(tokens.peek(), tokens,
                                                  diagnostics);
    } break;
    case TokenType::structure: {
        value = parse_struct_definition(tokens.pop(), tokens, diagnostics);
    } break;
    case TokenType::function:
    case TokenType::external: {
        value = parse_function_definition(tokens.pop(), tokens, diagnostics);
    } break;
    case TokenType::variable: {
        value = parse_variable_definition(tokens, diagnostics, tokens.pop());
    } break;
    case TokenType::string_literal: {
        auto token = tokens.pop();
        value = std::make_optional(
            Node{StringLiteral{std::get<std::string>(token.value), {token}}});
    } break;
    case TokenType::integer_literal: {
        auto token = tokens.pop();
        value = std::make_optional(Node{IntegerLiteral{
            stoull(std::get<std::string>(token.value)), {token}}});
    } break;
    default: {
        std::cerr << "Unexpected token: " << tokens.peek().type << '\n';
        return std::nullopt;
    } break;
    }

    if (!value.has_value()) {
        return std::nullopt;
    }

    if (peek_token_type(tokens, TokenType::dot)) {
        const auto dot_token = tokens.pop();
        const auto chained_expression = parse_expression(tokens, diagnostics);

        if (!chained_expression.has_value()) {
            diagnostics.push_error("Expected chained expression",
                                   dot_token.source);
            return std::nullopt;
        }

        value = Node{
            MemberAccess{std::make_shared<Node>(value.value()),
                         std::make_shared<Node>(chained_expression.value()),
                         {dot_token}}};
    }

    std::cerr << "Parsed expression " << value.value() << '\n';

    return value;
}

auto xlang::parse(Buffer<std::vector<Token>> tokens, Diagnostics& diagnostics)
    -> std::vector<Node> {
    std::vector<Node> expressions{};
    while (!tokens.empty()) {
        auto node = parse_expression(tokens, diagnostics);
        if (node.has_value()) {
            expressions.push_back(node.value());
        } else {
            tokens.safe_pop();
        }
    }
    return expressions;
}
