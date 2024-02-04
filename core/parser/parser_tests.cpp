#include "parser.h"
#include <gtest/gtest.h>
#include <optional>

using namespace xlang;
using namespace xlang;

TEST(ParserTest, TestParsing) {
    auto source = Source{};
    auto diagnostics = Diagnostics{};
    const std::vector<Token> tokens{
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
    auto ast = parse(tokens, diagnostics);
    auto expected = std::vector<Node>{{FunctionDefinition{
        "main",
        false,
        std::vector<FunctionDefinition::Parameter>{},
        std::nullopt,
        std::vector<Node>{{FunctionCall{
            "print",
            std::vector<Node>{StringLiteral{
                "Hello, world!",
                {Token{TokenType::string_literal, "Hello, world!", source}}}},
            {Token{TokenType::identifier, "print", source},
             Token{TokenType::paren_open, source},
             Token{TokenType::paren_close, source}}}}},
        nullptr,
        {
            std::nullopt,
            Token{TokenType::function, source},
            Token{TokenType::identifier, "main", source},
        }}}};
    ASSERT_EQ(ast, expected);
    ASSERT_EQ(diagnostics.size(), 0);
}
