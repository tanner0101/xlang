#include "lexer.h"
#include <gtest/gtest.h>

Lexer lexer{};

TEST(LexerTest, TestTokenization) {
    auto diagnostics = Diagnostics{};
    const auto parsed = lexer.lex(R"(
fn main() {
    print("Hello, world!")
}
)",
                                  diagnostics);

    auto source = Source{};
    const std::vector<Token> expected{
        Token{TokenType::function, source},
        Token{TokenType::identifier, "main", source},
        Token{TokenType::paren_open, source},
        Token{TokenType::paren_close, source},
        Token{TokenType::curly_open, source},
        Token{TokenType::identifier, "print", source},
        Token{TokenType::paren_open, source},
        Token{TokenType::string_literal, "Hello, world!", source},
        Token{TokenType::paren_close, source},
        Token{TokenType::curly_close, source}};

    ASSERT_EQ(parsed, expected);
    ASSERT_EQ(diagnostics.size(), 0);
}
