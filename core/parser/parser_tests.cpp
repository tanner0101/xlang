#include "parser.h"
#include <gtest/gtest.h>

Parser parser{};

TEST(ParserTest, TestParsing) {
    const std::vector<Token> tokens{
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
    auto ast = parser.parse(tokens);
    auto expected = std::vector<Node>{
        FunctionDefinition{"main", std::vector<Node>{},
                           std::vector<Node>{FunctionCall{
                               "print", std::vector<Node>{"Hello, world!"}}}}};
    ASSERT_EQ(ast, expected);
}
