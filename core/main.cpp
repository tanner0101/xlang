#include "ir/ir.h"
#include "lexer/lexer.h"
#include "llvmir/llvmir.h"
#include "parser/parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace xlang;

auto main(int argc, char* argv[]) -> int {
    std::vector<std::string> args(argv, argv + argc);

    std::istream* input = nullptr;
    std::ifstream file;

    if (argc == 1) {
        input = &std::cin;
    } else if (argc == 2) {
        file.open(args[1]);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << args[1] << '\n';
            return 1;
        }
        input = &file;
    } else {
        std::cerr << "Usage: " << args[0] << " [file]" << '\n';
        return 1;
    }

    std::stringstream buffer;
    buffer << input->rdbuf();
    std::string program = buffer.str();

    auto diagnostics = Diagnostics{};

    const auto tokens = lex(program, diagnostics);
    // for (const auto& token : tokens) {
    //     std::cerr << token << '\n';
    // }
    // return 0;

    const auto ast = parse(tokens, diagnostics);
    // for (const auto& node : ast) {
    //     std::cerr << node << '\n';
    // }
    // return 0;

    const auto module = ir::compile(ast, diagnostics);
    std::cerr << module << '\n';

    std::cout << llvmir::print(module, diagnostics) << '\n';

    // const auto ir = compile(ast, diagnostics);
    // std::cout << ir << '\n';

    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.message << " (" << diagnostic.source << ")"
                  << '\n';
    }

    return 0;
}
