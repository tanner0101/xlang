#include "lexer.h"
#include <gtest/gtest.h>

using namespace xlang;

TEST(LexerTest, TestTokenization) {
    auto diagnostics = Diagnostics{};
    const auto parsed = lex(R"(fn main() {
    var foo = "bar"
    print("Hello, world!")
})",
                            diagnostics);

    const std::vector<Token> expected{
        Token{TokenType::function, Source{0, 0}},
        Token{TokenType::identifier, "main", Source{0, 3}},
        Token{TokenType::paren_open, Source{0, 7}},
        Token{TokenType::paren_close, Source{0, 8}},
        Token{TokenType::curly_open, Source{0, 10}},
        Token{TokenType::variable, Source{1, 4}},
        Token{TokenType::identifier, "foo", Source{1, 8}},
        Token{TokenType::equal, Source{1, 12}},
        Token{TokenType::string_literal, "bar", Source{1, 14}},
        Token{TokenType::identifier, "print", Source{2, 4}},
        Token{TokenType::paren_open, Source{2, 9}},
        Token{TokenType::string_literal, "Hello, world!", Source{2, 10}},
        Token{TokenType::paren_close, Source{2, 25}},
        Token{TokenType::curly_close, Source{3, 0}},
    };

    ASSERT_EQ(parsed, expected);
    ASSERT_EQ(diagnostics.size(), 0);
}
