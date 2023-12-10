#include "compiler.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

auto generateIR(const Node& node, llvm::Module& module,
                llvm::IRBuilder<>& builder) -> llvm::Value* {
    switch (node.type) {
    case NodeType::function_definition: {
        auto& funcDef = std::get<FunctionDefinition>(node.value);
        std::vector<llvm::Type*> argTypes(
            funcDef.arguments.size(),
            llvm::Type::getInt32Ty(module.getContext()));
        auto* returnType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(module.getContext()), argTypes, false);
        auto* func = llvm::Function::Create(
            returnType, llvm::Function::ExternalLinkage, funcDef.name, module);
        auto* bb = llvm::BasicBlock::Create(module.getContext(), "entry", func);
        builder.SetInsertPoint(bb);
        for (const auto& stmt : funcDef.body) {
            generateIR(stmt, module, builder);
        }
        return builder.CreateRet(builder.getInt32(0));
    } break;
    case NodeType::function_call: {
        auto& funcCall = std::get<FunctionCall>(node.value);

        assert(funcCall.name == "print");

        // Declare printf function
        llvm::ArrayRef<llvm::Type*> printfArgs{
            builder.getInt8Ty()->getPointerTo()};
        llvm::FunctionType* printfType =
            llvm::FunctionType::get(builder.getInt32Ty(), printfArgs, true);
        llvm::Function* printfFunc = llvm::Function::Create(
            printfType, llvm::Function::ExternalLinkage, "printf", module);

        std::vector<llvm::Value*> args;
        for (const auto& arg : funcCall.arguments) {
            args.push_back(generateIR(arg, module, builder));
        }

        // Create a call to printf
        return builder.CreateCall(printfFunc, std::vector<llvm::Value*>{args});
    } break;
    case NodeType::string_literal: {
        return builder.CreateGlobalStringPtr(std::get<std::string>(node.value));
    }
    default: {
        // Ignore for now.
    } break;
    }

    return nullptr;
}

auto xlang::Compiler::compile(Buffer<std::vector<Node>> ast) -> std::string {
    llvm::LLVMContext context;
    llvm::Module module("xlang", context);
    llvm::IRBuilder<> builder(context);

    while (!ast.empty()) {
        const auto node = ast.pop();
        generateIR(node, module, builder);
    }

    std::string moduleStr;
    llvm::raw_string_ostream ostream(moduleStr);
    module.print(ostream, nullptr);
    return ostream.str();
}