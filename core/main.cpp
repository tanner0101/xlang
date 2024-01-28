#include "compiler/compiler.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace xlang;

auto main(int argc, char* argv[]) -> int {
    std::istream* input;
    std::ifstream file;

    if (argc == 1) {
        input = &std::cin;
    } else if (argc == 2) {
        file.open(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << argv[1] << std::endl;
            return 1;
        }
        input = &file;
    }

    std::stringstream buffer;
    buffer << input->rdbuf();
    std::string program = buffer.str();

    Lexer lexer{};
    Parser parser{};
    Compiler compiler{};
    auto diagnostics = Diagnostics{};

    const auto tokens = lexer.lex(program, diagnostics);
    // for (const auto& token : tokens) {
    //     std::cerr << token << std::endl;
    // }
    // return 0;

    const auto ast = parser.parse(tokens, diagnostics);
    // for (const auto& node : ast) {
    //     std::cerr << node << std::endl;
    // }
    // return 0;

    const auto ir = compiler.compile(ast, diagnostics);
    std::cout << ir << std::endl;

    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.message << " (" << diagnostic.source << ")"
                  << std::endl;
    }

    return 0;
}
