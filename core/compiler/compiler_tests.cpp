#include "compiler.h"
#include <gtest/gtest.h>

using namespace xlang;
using namespace xlang;
using namespace xlang;

Compiler compiler{};

TEST(CompilerTest, TestCompile) {
    auto diagnostics = Diagnostics();
    auto tokens = std::vector<Token>{Token{TokenType::identifier, Source{}}};
    auto ast = std::vector<Node>{{FunctionDefinition{
        "main",
        std::vector<FunctionDefinition::Parameter>{},
        std::vector<Node>{
            {FunctionCall{"print",
                          std::vector<Node>{{StringLiteral{
                              "Hello, world!",
                              {{Token(TokenType::string_literal, Source{})}}}}},
                          {Token(TokenType::identifier, "print", Source{})}}}},
        {Token(TokenType::function, Source{}),
         Token(TokenType::identifier, "main", Source{})}}}};
    const auto lli = compiler.compile(ast, diagnostics);
    ASSERT_EQ(lli, R"(; ModuleID = 'xlang'
source_filename = "xlang"

@0 = private unnamed_addr constant [14 x i8] c"Hello, world!\00", align 1

declare i32 @printf(i8*, ...)

define void @main() {
entry:
  %0 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @0, i32 0, i32 0))
  ret void
}
)");
}
