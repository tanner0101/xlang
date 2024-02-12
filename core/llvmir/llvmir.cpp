#include "core/llvmir/llvmir.h"
#include "core/ir/ir.h"
#include "core/util/diagnostics.h"
#include <llvm-17/llvm/IR/Constant.h>
#include <llvm-17/llvm/IR/Instructions.h>
#include <llvm-17/llvm/IR/LLVMContext.h>
#include <llvm-17/llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <string>

using namespace xlang;

auto translate_type(const std::shared_ptr<ir::Type>& type,
                    llvm::LLVMContext& context, Diagnostics& diagnostics)
    -> llvm::Type* {

    if (const auto void_type = std::dynamic_pointer_cast<ir::VoidType>(type)) {
        return llvm::Type::getVoidTy(context);
    }

    if (const auto primitive_type =
            std::dynamic_pointer_cast<ir::PrimitiveType>(type)) {
        switch (primitive_type->primitive) {
        case ir::Primitive::i8:
        case ir::Primitive::u8:
            return llvm::Type::getInt8Ty(context);
        case ir::Primitive::i16:
        case ir::Primitive::u16:
            return llvm::Type::getInt16Ty(context);
        case ir::Primitive::i32:
        case ir::Primitive::u32:
            return llvm::Type::getInt32Ty(context);
        case ir::Primitive::i64:
        case ir::Primitive::u64:
            return llvm::Type::getInt64Ty(context);
            return llvm::Type::getInt8Ty(context);
        case ir::Primitive::f32:
            return llvm::Type::getFloatTy(context);
        case ir::Primitive::f64:
            return llvm::Type::getDoubleTy(context);
        default:
            return nullptr;
        }
    }

    if (const auto pointer_type =
            std::dynamic_pointer_cast<ir::PointerType>(type)) {
        return translate_type(pointer_type->pointee, context, diagnostics)
            ->getPointerTo();
    }

    diagnostics.push_error("Unknown type", Source{});
    return nullptr;
}

auto translate_node(const std::shared_ptr<ir::IRNode>& node,
                    const ir::Module& module, llvm::Module& llvm_module,
                    llvm::IRBuilder<>& builder, Diagnostics& diagnostics)
    -> llvm::Value*;

auto translate_function(const std::shared_ptr<ir::Function>& function,
                        const ir::Module& module, llvm::Module& llvm_module,
                        Diagnostics& diagnostics) -> llvm::Function* {
    auto* const llvm_function = llvm::Function::Create(
        llvm::FunctionType::get(translate_type(function->return_type,
                                               llvm_module.getContext(),
                                               diagnostics),
                                false),
        llvm::Function::ExternalLinkage, function->name, &llvm_module);

    if (!function->definition.external) {
        auto* const entry = llvm::BasicBlock::Create(llvm_module.getContext(),
                                                     "entry", llvm_function);

        llvm::IRBuilder<> builder{llvm_module.getContext()};
        builder.SetInsertPoint(entry);
        for (const auto& node : function->body) {
            translate_node(node, module, llvm_module, builder, diagnostics);
        }
        if (function->return_value) {
            builder.CreateRet(translate_node(function->return_value, module,
                                             llvm_module, builder,
                                             diagnostics));
        } else {
            builder.CreateRetVoid();
        }
    }

    return llvm_function;
}

auto get_function(const std::string& name, const ir::Module& module,
                  llvm::Module& llvm_module, Diagnostics& diagnostics)
    -> llvm::Function* {
    auto* llvm_function = llvm_module.getFunction(name);
    if (llvm_function == nullptr) {
        if (!module.functions.contains(name)) {
            return nullptr;
        }

        llvm_function = translate_function(module.functions.at(name), module,
                                           llvm_module, diagnostics);
    }

    return llvm_function;
}

auto escape_string(const std::string& input) -> std::string {
    std::string output;
    bool escape = false;

    for (char ch : input) {
        if (escape) {
            switch (ch) {
            case 'n':
                output += '\n';
                break;
            case 't':
                output += '\t';
                break;
            // Add more cases here for other escape sequences if needed
            default:
                output +=
                    ch; // For unrecognized sequences, keep the character as-is
            }
            escape = false; // Reset the escape flag
        } else {
            if (ch == '\\') {
                escape = true; // Set flag if backslash is found
            } else {
                output += ch; // Add character to output as-is
            }
        }
    }

    // Append a trailing backslash if it's the last character in the input
    if (escape) {
        output += '\\';
    }

    return output;
}

auto translate_node(const std::shared_ptr<ir::IRNode>& node,
                    const ir::Module& module, llvm::Module& llvm_module,
                    llvm::IRBuilder<>& builder, Diagnostics& diagnostics)
    -> llvm::Value* {
    if (const auto string_literal_node =
            dynamic_pointer_cast<ir::StringLiteralIRNode>(node)) {
        return builder.CreateGlobalStringPtr(
            escape_string(string_literal_node->value.value));
    }

    if (const auto integer_literal_node =
            dynamic_pointer_cast<ir::IntegerLiteralIRNode>(node)) {
        return llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(builder.getContext()),
            integer_literal_node->value.value, false);
    }

    if (const auto function_call_node =
            dynamic_pointer_cast<ir::FunctionCallIRNode>(node)) {

        auto* const llvm_function =
            get_function(function_call_node->function->name, module,
                         llvm_module, diagnostics);
        if (llvm_function == nullptr) {
            // TODO: pass through source
            diagnostics.push_error("Unknown function", Source{});
            return nullptr;
        }

        std::vector<llvm::Value*> llvm_args{};
        llvm_args.reserve(function_call_node->arguments.size());
        for (const auto& arg : function_call_node->arguments) {
            llvm_args.push_back(
                translate_node(arg, module, llvm_module, builder, diagnostics));
        }
        return builder.CreateCall(llvm_function, llvm_args);
    }

    diagnostics.push_error("Unknown IR node type", Source{});
    return nullptr;
}

auto xlang::llvmir::print(const ir::Module& module, Diagnostics& diagnostics)
    -> std::string {
    llvm::LLVMContext context;
    llvm::Module llvm_module("xlang", context);

    if (!module.functions.contains("main")) {
        diagnostics.push_error("No main function", Source{});
        return "";
    }

    translate_function(module.functions.at("main"), module, llvm_module,
                       diagnostics);

    std::string module_str;
    llvm::raw_string_ostream ostream(module_str);
    llvm_module.print(ostream, nullptr);
    return ostream.str();
}