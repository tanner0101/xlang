#include "parser.h"
#include <gtest/gtest.h>

Parser parser{};

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
    auto ast = parser.parse(tokens, diagnostics);
    auto expected = std::vector<Node>{
        {FunctionDefinition{
             "main", std::vector<Node>{},
             std::vector<Node>{
                 {FunctionCall{"print",
                               std::vector<Node>{
                                   {"Hello, world!", std::vector<Token>{}}}},
                  std::vector<Token>{}}}},
         std::vector<Token>{}}};
    ASSERT_EQ(ast, expected);
    ASSERT_EQ(diagnostics.size(), 0);
}
