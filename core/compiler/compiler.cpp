#include "compiler.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace xlang;
using namespace xlang;

struct Type {
    std::string name;
    llvm::Type* llvm;
};

struct Function {
    std::string name;
    std::vector<Type*> parameterTypes;
    Type* returnType;
    const FunctionDefinition* definition;
    llvm::Function* llvm;
};

struct Variable {
    std::string name;
    Type type;
    llvm::Value* llvm;
};

struct Scope {
    std::unordered_map<std::string, Variable*> variables = {};
    std::unordered_map<std::string, Function*> functions = {};
    std::unordered_map<std::string, Type*> types = {};
    Scope* parent = nullptr;

    auto getVariable(const std::string& name) -> Variable* {
        if (variables.find(name) != variables.end()) {
            return variables[name];
        } else if (parent) {
            return parent->getVariable(name);
        } else {
            return nullptr;
        }
    }

    auto getFunction(const std::string& name) -> Function* {
        if (functions.find(name) != functions.end()) {
            return functions[name];
        } else if (parent) {
            return parent->getFunction(name);
        } else {
            return nullptr;
        }
    }

    auto getType(const std::string& name) -> Type* {
        if (types.find(name) != types.end()) {
            return types[name];
        } else if (parent) {
            return parent->getType(name);
        } else {
            return nullptr;
        }
    }

    auto push_local() -> Scope { return Scope{{}, {}, {}, this}; }
};

auto compileNode(const Node& node, llvm::Module& module,
                 llvm::IRBuilder<>& builder, Scope& scope,
                 Diagnostics& diagnostics) -> llvm::Value* {
    switch (node.type) {
    case NodeType::function_definition: {
        auto& funcDef = std::get<FunctionDefinition>(node.value);

        std::vector<llvm::Type*> llvmTypes{};
        std::vector<Type*> types{};
        for (const auto& param : funcDef.parameters) {
            auto type = scope.getType(param.type);
            if (!type) {
                diagnostics.push_error("No type named " + param.type + " found",
                                       param.tokens.type.source);
                return nullptr;
            }
            llvmTypes.push_back(type->llvm);
            types.push_back(type);
        }

        auto func = llvm::Function::Create(
            llvm::FunctionType::get(builder.getVoidTy(), llvmTypes, false),
            llvm::Function::ExternalLinkage, funcDef.name, module);

        auto args = func->arg_begin();
        auto nestedScope = scope.push_local();
        for (const auto& param : funcDef.parameters) {
            auto type = nestedScope.getType(param.type);
            auto value = args++;
            value->setName(param.name);
            auto variable = new Variable{param.name, *type, value};
            scope.variables[param.name] = variable;
        }

        auto* block =
            llvm::BasicBlock::Create(module.getContext(), "entry", func);
        builder.SetInsertPoint(block);

        for (const auto& stmt : funcDef.body) {
            compileNode(stmt, module, builder, nestedScope, diagnostics);
        }

        builder.CreateRetVoid();

        // TODO: memory management
        auto function = new Function{
            funcDef.name, types, scope.getType("Void"),
            new FunctionDefinition{funcDef.name, funcDef.parameters,
                                   funcDef.body, funcDef.tokens},
            func};
        scope.functions[funcDef.name] = function;
        return func;
    } break;
    case NodeType::function_call: {
        auto& funcCall = std::get<FunctionCall>(node.value);
        auto func = scope.getFunction(funcCall.name);

        std::vector<llvm::Value*> args;
        for (const auto& arg : funcCall.arguments) {
            args.push_back(
                compileNode(arg, module, builder, scope, diagnostics));
        }

        if (!func) {
            diagnostics.push_error("No function named " + funcCall.name +
                                       " found",
                                   funcCall.tokens.identifier.source);
            return nullptr;
        }

        if (args.size() != func->parameterTypes.size()) {
            diagnostics.push_error(
                "Incorrect number of arguments for function " + funcCall.name,
                funcCall.tokens.identifier.source);
            return nullptr;
        } else {
            for (std::size_t i = 0; i < args.size(); i++) {
                auto value = args[i];

                std::string paramName;
                if (func->definition) {
                    auto paramDef = func->definition->parameters[i];
                    paramName = paramDef.name;
                } else {
                    paramName = "param" + std::to_string(i);
                }

                auto paramType = func->parameterTypes[i];

                if (value->getType() != paramType->llvm) {
                    diagnostics.push_error("Incorrect type for parameter '" +
                                               paramName + "', expected " +
                                               paramType->name,
                                           funcCall.tokens.identifier.source);
                    return nullptr;
                }
            }
        }

        return builder.CreateCall(func->llvm, std::vector<llvm::Value*>{args});
    } break;
    case NodeType::string_literal: {
        return builder.CreateGlobalStringPtr(
            std::get<StringLiteral>(node.value).value);
    } break;
    case NodeType::integer_literal: {
        return llvm::ConstantInt::get(
            scope.getType("Int")->llvm,
            std::get<IntegerLiteral>(node.value).value, false);
    } break;
    case NodeType::identifier: {
        auto identifier = std::get<Identifier>(node.value);
        auto variable = scope.getVariable(identifier.name);
        if (!variable) {
            diagnostics.push_error("No variable named " + identifier.name +
                                       " found",
                                   identifier.token.source);
            return nullptr;
        }
        // return builder.CreateLoad(variable->llvm->getType(), variable->llvm,
        //                           identifier.name);
        return variable->llvm;
    } break;
    case NodeType::variable_definition: {
        auto& varDef = std::get<VariableDefinition>(node.value);
        auto* value =
            compileNode(*varDef.value, module, builder, scope, diagnostics);
        auto variable = new Variable{varDef.name, {"String"}, value};
        scope.variables[varDef.name] = variable;
    } break;
    default: {
        std::cerr << "Unknown node type: " << node.type << std::endl;
    } break;
    }

    return nullptr;
}

auto Compiler::compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
    -> std::string {
    llvm::LLVMContext context;
    llvm::Module module("xlang", context);
    llvm::IRBuilder<> builder(context);

    auto scope = Scope{{}, {}, {}, nullptr};

    scope.types["String"] =
        new Type{"String", builder.getInt8Ty()->getPointerTo()};
    scope.types["Int"] = new Type{"Int", builder.getInt64Ty()};
    scope.types["Void"] = new Type{"Void", builder.getVoidTy()};

    scope.functions["print"] =
        new Function{"print",
                     {scope.getType("String")},
                     scope.getType("Void"),
                     nullptr,
                     llvm::Function::Create(
                         llvm::FunctionType::get(
                             builder.getInt32Ty(),
                             {builder.getInt8Ty()->getPointerTo()}, true),
                         llvm::Function::ExternalLinkage, "printf", module)};

    while (!ast.empty()) {
        const auto node = ast.pop();
        compileNode(node, module, builder, scope, diagnostics);
    }

    std::string moduleStr;
    llvm::raw_string_ostream ostream(moduleStr);
    module.print(ostream, nullptr);
    return ostream.str();
}