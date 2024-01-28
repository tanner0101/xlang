#include "compiler.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <sstream>
#include <string>

using namespace xlang;
using namespace xlang;

struct Type {
    std::string name;
    llvm::Type* llvm;
    const StructDefinition* definition = nullptr;
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
    Type* type;
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

    auto getType(const TypeIdentifier& type) -> Type* {
        if (type.name == "Pointer") {
            // TODO: this is so dumb
            auto internal = getType(type.genericParameters[0]);
            return getTypeByLlvm(internal->llvm->getPointerTo());
        }

        if (types.find(type.name) != types.end()) {
            return types[type.name];
        } else if (parent) {
            return parent->getType(type);
        } else {
            return nullptr;
        }
    }

    auto _getType(const std::string& name) -> Type* {
        if (types.find(name) != types.end()) {
            return types[name];
        } else if (parent) {
            return parent->_getType(name);
        } else {
            return nullptr;
        }
    }

    auto getTypeByLlvm(const llvm::Type* llvm) -> Type* {
        for (const auto& type : types) {
            if (type.second->llvm == llvm) {
                return type.second;
            }
        }
        if (parent) {
            return parent->getTypeByLlvm(llvm);
        }

        if (llvm->isPointerTy()) {
            // TODO: hack to support structs being pass by reference
            return getTypeByLlvm(llvm->getPointerElementType());
        }

        return _getType("Void");
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
                diagnostics.push_error("No type named " + param.type.name +
                                           " found",
                                       param.tokens.identifier.source);
                return nullptr;
            }
            if (type->definition) {
                llvmTypes.push_back(type->llvm->getPointerTo());
            } else {
                llvmTypes.push_back(type->llvm);
            }
            types.push_back(type);
        }

        // TODO: support return types
        auto func = llvm::Function::Create(
            llvm::FunctionType::get(builder.getInt32Ty(), llvmTypes, false),
            llvm::Function::ExternalLinkage, funcDef.name, module);

        auto args = func->arg_begin();
        auto nestedScope = scope.push_local();
        for (const auto& param : funcDef.parameters) {
            auto type = nestedScope.getType(param.type);
            auto value = args++;
            auto variable = new Variable{param.name, type, value};
            scope.variables[param.name] = variable;
        }

        if (!funcDef.external) {
            auto* block =
                llvm::BasicBlock::Create(module.getContext(), "entry", func);
            builder.SetInsertPoint(block);

            for (const auto& stmt : funcDef.body) {
                compileNode(stmt, module, builder, nestedScope, diagnostics);
            }

            // TODO: support other return types
            builder.CreateRet(llvm::ConstantInt::get(builder.getInt32Ty(), 0));
        }

        // TODO: memory management
        auto function = new Function{
            funcDef.name, types, scope._getType("Void"),
            new FunctionDefinition{funcDef.name, false, funcDef.parameters,
                                   funcDef.body, funcDef.tokens},
            func};
        scope.functions[funcDef.name] = function;
        return func;
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
        if (identifier.next) {
            if (!variable->type->definition) {
                diagnostics.push_error("Cannot access member of non-struct " +
                                           variable->type->name,
                                       identifier.token.source);
                return nullptr;
            }

            int found = -1;
            int i = 0;
            for (const auto& member : variable->type->definition->members) {
                if (member.name == identifier.next->name) {
                    found = i;
                    break;
                }
                i++;
            }

            if (found == -1) {
                diagnostics.push_error(
                    "No member named '" + identifier.next->name +
                        "' found on type " + variable->type->name + ".",
                    identifier.next->token.source);
                return nullptr;
            }

            // TODO: support deeper nesting
            // TODO: support actually looking up the member
            llvm::Value* memberPtr = builder.CreateStructGEP(
                variable->type->llvm, variable->llvm, found, "memberPtr");
            // TODO: support other types
            return builder.CreateLoad(builder.getInt8PtrTy(), memberPtr,
                                      identifier.name);
        } else {
            return variable->llvm;
        }
    } break;
    case NodeType::function_call: {
        auto& funcCall = std::get<FunctionCall>(node.value);
        auto func = scope.getFunction(funcCall.name);

        std::vector<llvm::Value*> args;
        for (const auto& arg : funcCall.arguments) {
            const auto llvm_arg =
                compileNode(arg, module, builder, scope, diagnostics);
            if (!llvm_arg) {
                // error already reported
                return nullptr;
            }
            args.push_back(llvm_arg);
        }

        if (!func) {
            diagnostics.push_error("No function named '" + funcCall.name +
                                       "' found.",
                                   funcCall.tokens.identifier.source);
            return nullptr;
        }

        if (args.size() != func->parameterTypes.size()) {
            diagnostics.push_error(
                "Incorrect number of arguments for function '" + funcCall.name +
                    "'. Expected " +
                    std::to_string(func->parameterTypes.size()) + ", got " +
                    std::to_string(args.size()) + ".",
                funcCall.tokens.parenClose.source);
            return nullptr;
        } else {
            for (std::size_t i = 0; i < args.size(); i++) {
                auto value = args[i];

                std::string paramName;
                Source source;
                if (func->definition) {
                    auto paramDef = func->definition->parameters[i];
                    paramName = paramDef.name;
                    source = node_source(funcCall.arguments[i]);
                } else {
                    paramName = "param" + std::to_string(i);
                    source = funcCall.tokens.identifier.source;
                }

                auto paramType = func->parameterTypes[i];

                auto actualType = scope.getTypeByLlvm(value->getType());
                if (actualType != paramType) {
                    diagnostics.push_error("Incorrect type for parameter '" +
                                               paramName + "'. Expected " +
                                               paramType->name + ", got " +
                                               actualType->name + ".",
                                           source);
                    return nullptr;
                }
            }
        }

        return builder.CreateCall(func->llvm, std::vector<llvm::Value*>{args});
    } break;
    case NodeType::struct_definition: {
        auto& value = std::get<StructDefinition>(node.value);
        auto llvm_members = std::vector<llvm::Type*>{};
        for (const auto& member : value.members) {
            auto type = scope.getType(member.type);
            if (!type) {
                diagnostics.push_error("No type named " + member.type.name +
                                           " found",
                                       member.tokens.name.source);
                return nullptr;
            }
            llvm_members.push_back(type->llvm);
        }
        auto llvm_value = llvm::StructType::create(
            module.getContext(), llvm_members, value.name, false);

        scope.types[value.name] = new Type{
            value.name, llvm_value,
            new StructDefinition{value.name, value.members, value.tokens}};
    } break;
    case NodeType::string_literal: {
        auto value = std::get<StringLiteral>(node.value);
        auto stringType = scope._getType("String");
        if (!stringType) {
            diagnostics.push_error("No type named String found",
                                   value.token.source);
            return nullptr;
        }

        const auto alloca =
            builder.CreateAlloca(stringType->llvm, nullptr, "literal");

        builder.CreateStore(
            builder.CreateGlobalStringPtr(
                std::get<StringLiteral>(node.value).value),
            builder.CreateStructGEP(stringType->llvm, alloca, 0));

        return alloca;
    } break;
    case NodeType::integer_literal: {
        return llvm::ConstantInt::get(
            scope._getType("Int")->llvm,
            std::get<IntegerLiteral>(node.value).value, false);
    } break;
    case NodeType::variable_definition: {
        auto& varDef = std::get<VariableDefinition>(node.value);
        auto* value =
            compileNode(*varDef.value, module, builder, scope, diagnostics);
        // TODO: get struct type from compile node somehow
        auto variable =
            new Variable{varDef.name, scope._getType("String"), value};
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

    // scope.types["String"] =
    //     new Type{"String", builder.getInt8Ty()->getPointerTo()};
    scope.types["Int"] = new Type{"Int", builder.getInt64Ty()};
    scope.types["Int32"] = new Type{"Int32", builder.getInt32Ty()};
    scope.types["UInt8"] = new Type{"UInt8", builder.getInt8Ty()};
    scope.types["Pointer<UInt8>"] =
        new Type{"Pointer<UInt8>", builder.getInt8Ty()->getPointerTo()};
    scope.types["Void"] = new Type{"Void", builder.getVoidTy()};

    while (!ast.empty()) {
        const auto node = ast.pop();
        compileNode(node, module, builder, scope, diagnostics);
    }

    std::string moduleStr;
    llvm::raw_string_ostream ostream(moduleStr);
    module.print(ostream, nullptr);
    return ostream.str();
}