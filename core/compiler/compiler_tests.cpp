#include "compiler.h"
#include <gtest/gtest.h>

xlang::Compiler compiler{};

TEST(CompilerTest, TestCompile) {
    auto diagnostics = Diagnostics();
    auto tokens = std::vector<Token>{Token{TokenType::identifier, Source{}}};
    auto ast = std::vector<Node>{
        {FunctionDefinition{
             "main", std::vector<Node>{},
             std::vector<Node>{
                 {FunctionCall{"printf",
                               std::vector<Node>{{"Hello, world!", tokens}}},
                  tokens}}},
         tokens}};
    const auto lli = compiler.compile(ast, diagnostics);
    ASSERT_EQ(lli, R"(; ModuleID = 'xlang'
source_filename = "xlang"

@0 = private unnamed_addr constant [14 x i8] c"Hello, world!\00", align 1

declare i32 @printf(i8*, ...)

define i32 @main(i32 %0) {
entry:
  %1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @0, i32 0, i32 0))
  ret i32 0
}
)");
}
