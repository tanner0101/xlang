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
    lexer.lex(program);
    const auto tokens = lexer.lex(program);
    const auto ast = parser.parse(tokens);
    std::cout << compiler.compile(ast) << std::endl;
    return 0;
}
