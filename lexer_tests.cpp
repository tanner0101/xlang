#include "lexer.h"
#include <gtest/gtest.h>

Lexer lexer{};

TEST(LexerTest, TestTokenization) {
    const auto parsed = lexer.lex(R"(
fn main() {
    print("Hello, world!")
}
)");

    const std::vector<Token> expected{
        Token{TokenType::function},
        Token{TokenType::identifier, "main"},
        Token{TokenType::paren_open},
        Token{TokenType::paren_close},
        Token{TokenType::curly_open},
        Token{TokenType::identifier, "print"},
        Token{TokenType::paren_open},
        Token{TokenType::string_literal, "Hello, world!"},
        Token{TokenType::paren_close},
        Token{TokenType::curly_close}};

    ASSERT_EQ(parsed, expected);
}
