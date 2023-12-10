#include "parser.h"
#include <gtest/gtest.h>

Parser parser{};

TEST(ParserTest, TestParsing) {
    auto source = Source{};
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
    auto ast = parser.parse(tokens);
    auto expected = std::vector<Node>{
        FunctionDefinition{"main", std::vector<Node>{},
                           std::vector<Node>{FunctionCall{
                               "print", std::vector<Node>{"Hello, world!"}}}}};
    ASSERT_EQ(ast, expected);
}
