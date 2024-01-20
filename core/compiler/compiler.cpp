#include "compiler.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace xlang;
using namespace xlang;

struct Scope {
    std::unordered_map<std::string, llvm::Value*> variables = {};
    Scope* parent = nullptr;

    auto get_variable(const std::string& name) -> llvm::Value* {
        if (variables.find(name) != variables.end()) {
            return variables[name];
        } else if (parent) {
            return parent->get_variable(name);
        } else {
            return nullptr;
        }
    }

    auto push_local() -> Scope { return Scope{{}, this}; }
};

auto generateIR(const Node& node, llvm::Module& module,
                llvm::IRBuilder<>& builder, Scope& scope,
                Diagnostics& diagnostics) -> llvm::Value* {
    std::cerr << node << std::endl;
    switch (node.type) {
    case NodeType::function_definition: {
        auto& funcDef = std::get<FunctionDefinition>(node.value);

        llvm::Function* func;
        if (funcDef.name == "main") {
            func = llvm::Function::Create(
                llvm::FunctionType::get(builder.getInt32Ty(),
                                        {builder.getInt32Ty()}, false),
                llvm::Function::ExternalLinkage, funcDef.name, module);
        } else {
            func = llvm::Function::Create(
                llvm::FunctionType::get(builder.getVoidTy(), false),
                llvm::Function::PrivateLinkage, funcDef.name, module);
        }

        auto* block =
            llvm::BasicBlock::Create(module.getContext(), "entry", func);
        builder.SetInsertPoint(block);
        auto nestedScope = scope.push_local();
        for (const auto& stmt : funcDef.body) {
            generateIR(stmt, module, builder, nestedScope, diagnostics);
        }

        if (funcDef.name == "main") {
            return builder.CreateRet(builder.getInt32(0));
        } else {
            return builder.CreateRetVoid();
        }
    } break;
    case NodeType::function_call: {
        auto& funcCall = std::get<FunctionCall>(node.value);
        auto func = module.getFunction(funcCall.name);

        std::vector<llvm::Value*> args;
        for (const auto& arg : funcCall.arguments) {
            args.push_back(
                generateIR(arg, module, builder, scope, diagnostics));
        }

        if (!func) {
            diagnostics.push_error("No function named " + funcCall.name +
                                       " found",
                                   node.tokens[0].source);
            return nullptr;
        }

        return builder.CreateCall(func, std::vector<llvm::Value*>{args});
    } break;
    case NodeType::string_literal: {
        return builder.CreateGlobalStringPtr(std::get<std::string>(node.value));
    } break;
    case NodeType::identifier: {
        auto variable = scope.get_variable(std::get<std::string>(node.value));
        if (!variable) {
            diagnostics.push_error("No variable named " +
                                       std::get<std::string>(node.value) +
                                       " found",
                                   node.tokens[0].source);
            return nullptr;
        }
    } break;
    default: {
        // Ignore for now.
    } break;
    }

    return nullptr;
}

auto Compiler::compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
    -> std::string {
    llvm::LLVMContext context;
    llvm::Module module("xlang", context);
    llvm::IRBuilder<> builder(context);

    module.getOrInsertFunction(
        "printf",
        llvm::FunctionType::get(builder.getInt32Ty(),
                                {builder.getInt8Ty()->getPointerTo()}, true));

    auto scope = Scope{{}, nullptr};

    while (!ast.empty()) {
        const auto node = ast.pop();
        generateIR(node, module, builder, scope, diagnostics);
    }

    std::string moduleStr;
    llvm::raw_string_ostream ostream(moduleStr);
    module.print(ostream, nullptr);
    return ostream.str();
}