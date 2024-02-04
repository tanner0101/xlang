#include "lexer.h"

using namespace xlang;

ENUM_CLASS(LexerState, none, identifier, string_literal, integer_literal);

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto xlang::lex(Buffer<std::string> input, Diagnostics& diagnostics)
    -> std::vector<Token> {
    std::vector<Token> tokens{};

    std::string identifier{};

    auto source = Source{
        .line = 0,
        .column = 0,
    };

    LexerState state = LexerState::none;

    while (!input.empty()) {
        switch (state) {
        case LexerState::none:
            switch (input.peek()) {
            case '(':
                input.pop();
                tokens.emplace_back(TokenType::paren_open, source);
                source.column += 1;
                break;
            case ')':
                input.pop();
                tokens.emplace_back(TokenType::paren_close, source);
                source.column += 1;
                break;
            case '{':
                input.pop();
                tokens.emplace_back(TokenType::curly_open, source);
                source.column += 1;
                break;
            case '}':
                input.pop();
                tokens.emplace_back(TokenType::curly_close, source);
                source.column += 1;
                break;
            case '=':
                input.pop();
                tokens.emplace_back(TokenType::equal, source);
                source.column += 1;
                break;
            case ':':
                input.pop();
                tokens.emplace_back(TokenType::colon, source);
                source.column += 1;
                break;
            case ',':
                input.pop();
                tokens.emplace_back(TokenType::comma, source);
                source.column += 1;
                break;
            case '.':
                input.pop();
                tokens.emplace_back(TokenType::dot, source);
                source.column += 1;
                break;
            case '<':
                input.pop();
                tokens.emplace_back(TokenType::angle_open, source);
                source.column += 1;
                break;
            case '>':
                input.pop();
                tokens.emplace_back(TokenType::angle_close, source);
                source.column += 1;
                break;
            case '"':
                state = LexerState::string_literal;
                input.pop();
                break;
            case ' ':
                input.pop();
                source.column += 1;
                break;
            case '\n':
                input.pop();
                // tokens.emplace_back(TokenType::new_line, source);
                source.line += 1;
                source.column = 0;
                break;
            case '\t':
                diagnostics.push_error("Tabs are not allowed", source);
                break;
            default: {
                if (std::isalpha(input.peek()) != 0) {
                    state = LexerState::identifier;
                } else if (std::isdigit(input.peek()) != 0) {
                    state = LexerState::integer_literal;
                } else {
                    const auto unknown = input.pop();
                    tokens.emplace_back(TokenType::unknown,
                                        std::string(1, unknown), source);
                    state = LexerState::none;
                    diagnostics.push_error("Unknown token: '" +
                                               std::string(1, unknown) + "'",
                                           source);
                    source.column += 1;
                }
            } break;
            }
            break;
        case LexerState::identifier:
            if (std::isalnum(input.peek()) != 0) {
                identifier.push_back(input.pop());
            } else {
                if (identifier == "fn") {
                    tokens.emplace_back(TokenType::function, source);
                } else if (identifier == "extern") {
                    tokens.emplace_back(TokenType::external, source);
                } else if (identifier == "var") {
                    tokens.emplace_back(TokenType::variable, source);
                } else if (identifier == "struct") {
                    tokens.emplace_back(TokenType::structure, source);
                } else {
                    tokens.emplace_back(TokenType::identifier, identifier,
                                        source);
                }
                source.column += (int)identifier.size();
                identifier.clear();
                state = LexerState::none;
            }
            break;
        case LexerState::string_literal:
            // TODO: support escape sequences
            if (input.peek() == '"') {
                input.pop();
                state = LexerState::none;
                tokens.emplace_back(TokenType::string_literal, identifier,
                                    source);
                source.column += (int)identifier.size() + 2;
                identifier.clear();
            } else {
                identifier.push_back(input.pop());
            }
            break;
        case LexerState::integer_literal: {
            if (std::isdigit(input.peek()) != 0) {
                identifier.push_back(input.pop());
            } else {
                state = LexerState::none;
                tokens.emplace_back(TokenType::integer_literal, identifier,
                                    source);
                source.column += (int)identifier.size();
                identifier.clear();
            }
        } break;
        default:
            std::cerr << "Unknown lexer state: " << state << '\n';
            break;
        };
    }

    return tokens;
}
