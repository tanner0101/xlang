#include "lexer.h"

auto Lexer::lex(Buffer<std::string> input) -> std::vector<Token> {
    std::vector<Token> tokens{};

    std::string identifier{};

    while (!input.empty()) {
        switch (state) {
        case LexerState::none:
            switch (input.peek()) {
            case '(':
                input.pop();
                tokens.emplace_back(TokenType::paren_open);
                break;
            case ')':
                input.pop();
                tokens.emplace_back(TokenType::paren_close);
                break;
            case '{':
                input.pop();
                tokens.emplace_back(TokenType::curly_open);
                break;
            case '}':
                input.pop();
                tokens.emplace_back(TokenType::curly_close);
                break;
            case '"':
                input.pop();
                state = LexerState::string_literal;
                break;
            case ' ':
            case '\n':
            case '\t':
                input.pop();
                break;
            default:
                state = LexerState::identifier;
                break;
            }
            break;
        case LexerState::identifier:
            if (std::isalpha(input.peek())) {
                identifier.push_back(input.pop());
            } else {
                if (identifier == "fn") {
                    tokens.emplace_back(TokenType::function);
                } else {
                    tokens.emplace_back(TokenType::identifier, identifier);
                }
                identifier.clear();
                state = LexerState::none;
            }
            break;
        case LexerState::string_literal:
            // TODO: support escape sequences
            if (input.peek() == '"') {
                input.pop();
                state = LexerState::none;
                tokens.emplace_back(TokenType::string_literal, identifier);
                identifier.clear();
            } else {
                identifier.push_back(input.pop());
            }
            break;
        };
    }

    return tokens;
}
