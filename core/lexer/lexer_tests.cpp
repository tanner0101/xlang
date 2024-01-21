#include "lexer.h"
#include <gtest/gtest.h>

using namespace xlang;

Lexer lexer{};

TEST(LexerTest, TestTokenization) {
    auto diagnostics = Diagnostics{};
    const auto parsed = lexer.lex(R"(
fn main() {
    var foo = "bar"
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
        Token{TokenType::variable, source},
        Token{TokenType::identifier, "foo", source},
        Token{TokenType::assignment, source},
        Token{TokenType::string_literal, "bar", source},
        Token{TokenType::identifier, "print", source},
        Token{TokenType::paren_open, source},
        Token{TokenType::string_literal, "Hello, world!", source},
        Token{TokenType::paren_close, source},
        Token{TokenType::curly_close, source}};

    ASSERT_EQ(parsed, expected);
    ASSERT_EQ(diagnostics.size(), 0);
}
