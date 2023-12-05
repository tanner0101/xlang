#include "lexer.h"

Lexer::Lexer() {}

Lexer::~Lexer() {}

std::vector<Token> Lexer::lex(StringLexer input) {
    std::vector<Token> tokens{};

    std::string identifier{};

    while (input.peek().has_value()) {
        switch (state) {
        case LexerState::none:
            switch (input.peek().value()) {
            case '(':
                input.pop();
                tokens.push_back(Token{TokenType::paren_open});
                break;
            case ')':
                input.pop();
                tokens.push_back(Token{TokenType::paren_close});
                break;
            case '{':
                input.pop();
                tokens.push_back(Token{TokenType::curly_open});
                break;
            case '}':
                input.pop();
                tokens.push_back(Token{TokenType::curly_close});
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
            if (std::isalpha(input.peek().value())) {
                identifier.push_back(input.pop().value());
            } else {
                if (identifier == "fn") {
                    tokens.push_back(Token{TokenType::function});
                } else {
                    tokens.push_back(Token{TokenType::identifier, identifier});
                }
                identifier.clear();
                state = LexerState::none;
            }
            break;
        case LexerState::string_literal:
            // TODO: support escape sequences
            if (input.peek().value() == '"') {
                input.pop();
                state = LexerState::none;
                tokens.push_back(Token{TokenType::string_literal, identifier});
                identifier.clear();
            } else {
                identifier.push_back(input.pop().value());
            }
            break;
        };
    }

    return tokens;
}
