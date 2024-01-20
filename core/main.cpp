#include "compiler/compiler.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <iostream>

using namespace xlang;

auto main() -> int {
    Lexer lexer{};
    Parser parser{};
    Compiler compiler{};

    std::string program, line;
    while (std::getline(std::cin, line)) {
        program += line + "\n";
    }

    auto diagnostics = Diagnostics{};

    const auto tokens = lexer.lex(program, diagnostics);
    const auto ast = parser.parse(tokens, diagnostics);
    const auto ir = compiler.compile(ast, diagnostics);
    std::cout << ir << std::endl;

    // TODO: make diagnostics an iterable
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.message << " (" << diagnostic.source << ")"
                  << std::endl;
    }

    return 0;
}
