#include "compiler/compiler.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <iostream>

auto main() -> int {
    Lexer lexer{};
    Parser parser{};
    xlang::Compiler compiler{};

    std::string program, line;
    while (std::getline(std::cin, line)) {
        program += line + "\n";
    }

    const auto tokens = lexer.lex(program);
    const auto ast = parser.parse(tokens);
    const auto ir = compiler.compile(ast);
    std::cout << ir << std::endl;

    return 0;
}
