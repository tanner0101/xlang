#include "compiler.h"
#include "core/parser/node.h"
#include "core/util/diagnostics.h"
#include <cstddef>
#include <llvm-14/llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <optional>
#include <string>

using namespace xlang;
using namespace xlang;

struct Type;

struct Function {
    std::string name;
    std::vector<std::shared_ptr<Type>> parameter_types;
    std::shared_ptr<Type> return_type;
    const FunctionDefinition* definition;
    llvm::Function* llvm;
};

struct Variable {
    std::string name;
    std::shared_ptr<Type> type;
    llvm::Value* llvm;
    Source source;
    int uses = 0;
};

struct Type {
    std::string name;
    llvm::Type* llvm;
    enum class Semantic { value, reference };
    Semantic semantic = Semantic::value;
    const StructDefinition* definition = nullptr;
};

struct Scope {
    std::unordered_map<std::string, std::shared_ptr<Variable>> variables = {};
    std::unordered_map<std::string, std::shared_ptr<Function>> functions = {};
    std::unordered_map<std::string, std::shared_ptr<Type>> types = {};
    Scope* parent = nullptr;

    auto get_variable(const std::string& name) -> std::shared_ptr<Variable> {
        if (variables.find(name) != variables.end()) {
            return variables[name];
        }
        if (parent != nullptr) {
            return parent->get_variable(name);
        }
        return nullptr;
    }

    auto get_function(const std::string& name) -> std::shared_ptr<Function> {
        if (functions.find(name) != functions.end()) {
            return functions[name];
        }
        if (parent != nullptr) {
            return parent->get_function(name);
        }
        return nullptr;
    }

    auto get_type(const TypeIdentifier& type) -> std::shared_ptr<Type> {
        if (type.name == "Pointer") {
            // TODO: this is so dumb
            auto internal = get_type(type.genericParameters[0]);
            return get_type_by_llvm(internal->llvm->getPointerTo());
        }

        if (types.find(type.name) != types.end()) {
            return types[type.name];
        }

        if (parent != nullptr) {
            return parent->get_type(type);
        }

        return nullptr;
    }

    auto get_type_by_name(const std::string& name) -> std::shared_ptr<Type> {
        if (types.find(name) != types.end()) {
            return types[name];
        }
        if (parent != nullptr) {
            return parent->get_type_by_name(name);
        }
        return nullptr;
    }

    auto get_type_by_llvm(const llvm::Type* llvm) -> std::shared_ptr<Type> {
        for (const auto& type : types) {
            if (type.second->llvm == llvm) {
                return type.second;
            }
        }
        if (parent != nullptr) {
            return parent->get_type_by_llvm(llvm);
        }

        if (llvm->isPointerTy()) {
            // TODO: hack to support structs being pass by reference
            return get_type_by_llvm(llvm->getPointerElementType());
        }

        for (const auto& type : types) {
            std::cerr << type.first << '\n';
            type.second->llvm->dump();
        }
        llvm->dump();
        return nullptr;
    }

    auto push_local() -> Scope { return Scope{{}, {}, {}, this}; }
};

auto compile_node(const Node& node, llvm::Module& module,
                  llvm::IRBuilder<>& builder, Scope& scope,
                  Diagnostics& diagnostics) -> llvm::Value*;

auto warn_unused_variables(const Scope& scope, Diagnostics& diagnostics)
    -> void {
    for (const auto& variable : scope.variables) {
        if (variable.second->uses == 0) {
            diagnostics.push_warning("Unused variable '" + variable.first +
                                         "'.",
                                     variable.second->source);
        }
    }
}

auto compile_function_definition(const FunctionDefinition& value,
                                 llvm::Module& module,
                                 llvm::IRBuilder<>& builder, Scope& scope,
                                 Diagnostics& diagnostics) -> llvm::Value* {

    if (scope.functions.find(value.name) != scope.functions.end()) {
        diagnostics.push_error("Function '" + value.name +
                                   "' is already defined.",
                               value.tokens.identifier.source);
        return nullptr;
    }

    std::vector<llvm::Type*> llvm_types{};
    std::vector<std::shared_ptr<Type>> types{};
    for (const auto& param : value.parameters) {
        auto type = scope.get_type(param.type);
        if (type == nullptr) {
            diagnostics.push_error("No type named " + param.type.name +
                                       " found",
                                   param.tokens.identifier.source);
            return nullptr;
        }
        if (type->definition != nullptr) {
            llvm_types.push_back(type->llvm->getPointerTo());
        } else {
            llvm_types.push_back(type->llvm);
        }
        types.push_back(type);
    }

    // TODO: support return types
    auto* func = llvm::Function::Create(
        llvm::FunctionType::get(builder.getInt32Ty(), llvm_types, false),
        llvm::Function::ExternalLinkage, value.name, module);

    auto args = func->arg_begin(); // NOLINT(readability-qualified-auto)
    auto nested_scope = scope.push_local();
    for (const auto& param : value.parameters) {
        auto type = nested_scope.get_type(param.type);
        auto* llvm_var = args++;
        llvm_var->setName(param.name);
        auto variable = Variable{param.name, type, llvm_var,
                                 param.tokens.identifier.source};
        if (value.external) {
            variable.uses = 1;
        }
        scope.variables[param.name] = std::make_shared<Variable>(variable);
    }

    if (!value.external) {
        auto* block =
            llvm::BasicBlock::Create(module.getContext(), "entry", func);
        builder.SetInsertPoint(block);

        for (const auto& stmt : value.body) {
            compile_node(stmt, module, builder, nested_scope, diagnostics);
        }

        // TODO: support other return types
        builder.CreateRet(llvm::ConstantInt::get(builder.getInt32Ty(), 0));
    }

    warn_unused_variables(nested_scope, diagnostics);

    // TODO: memory management
    auto function =
        Function{value.name, types, scope.get_type_by_name("Void"),
                 new FunctionDefinition{value.name, false, value.parameters,
                                        value.body, value.tokens},
                 func};
    scope.functions[value.name] = std::make_shared<Function>(function);
    return func;
}

auto get_member_index(const Type& type, const std::string& name,
                      Diagnostics& diagnostics, const Source& source)
    -> std::optional<std::size_t> {
    int struct_index = 0;
    {
        if (type.definition == nullptr) {
            diagnostics.push_error("Type " + type.name + " is not a struct.",
                                   source);
            return std::nullopt;
        }
        int found = -1;
        int i = 0;
        for (const auto& member : type.definition->members) {
            if (member.name == name) {
                found = i;
                break;
            }
            i++;
        }

        if (found == -1) {
            diagnostics.push_error("No member named '" + name +
                                       "' found on type " + type.name + ".",
                                   source);
            return std::nullopt;
        }
        struct_index = found;
    }

    return struct_index;
}
auto compile_type_init(const Type& type, const FunctionCall& value,
                       std::vector<llvm::Value*> args, llvm::Module& module,
                       llvm::IRBuilder<>& builder, Scope& scope,
                       Diagnostics& diagnostics) -> llvm::Value* {

    auto* const alloca =
        builder.CreateAlloca(type.llvm, nullptr, type.name + "_init");

    if (value.arguments.size() != type.definition->members.size()) {
        diagnostics.push_error(
            "Incorrect number of arguments for type '" + type.name +
                "'. Expected " +
                std::to_string(type.definition->members.size()) + ", got " +
                std::to_string(value.arguments.size()) + ".",
            value.tokens.parenClose.source);
        return nullptr;
    }

    const auto source = value.tokens.identifier.source;

    for (std::size_t i = 0; i < value.arguments.size(); i++) {
        const auto& member = type.definition->members[i];
        auto* const llvm_arg = args[i];
        if (llvm_arg == nullptr) {
            // error already reported
            return nullptr;
        }

        const auto arg_type = scope.get_type_by_llvm(llvm_arg->getType());
        if (!arg_type) {
            diagnostics.push_error("No type found for argument", source);
            return nullptr;
        }
        const auto member_type = scope.get_type(member.type);
        if (!member_type) {
            diagnostics.push_error("No type found for member", source);
            return nullptr;
        }

        if (arg_type != member_type) {
            diagnostics.push_error("Incorrect type for member '" + member.name +
                                       "'. Expected " + member_type->name +
                                       ", got " + arg_type->name + ".",
                                   source);
            return nullptr;
        }

        builder.CreateStore(
            builder.CreateLoad(llvm_arg->getType()->getPointerElementType(),
                               llvm_arg),
            builder.CreateStructGEP(type.llvm, alloca, i));
    }

    return alloca;
}

auto compile_function_call(const FunctionCall& value, llvm::Module& module,
                           llvm::IRBuilder<>& builder, Scope& scope,
                           Diagnostics& diagnostics) -> llvm::Value* {

    std::vector<llvm::Value*> args;
    for (const auto& arg : value.arguments) {
        auto* const llvm_arg =
            compile_node(arg, module, builder, scope, diagnostics);
        if (llvm_arg == nullptr) {
            // error already reported
            return nullptr;
        }
        args.push_back(llvm_arg);
    }

    auto func = scope.get_function(value.name);
    if (!func) {
        const auto& type = scope.get_type_by_name(value.name);
        if (type) {
            return compile_type_init(*type, value, args, module, builder, scope,
                                     diagnostics);
        }

        diagnostics.push_error("No function or type named '" + value.name +
                                   "' found.",
                               value.tokens.identifier.source);
        return nullptr;
    }

    auto type = scope.get_type_by_name(value.name);

    if (args.size() != func->parameter_types.size()) {
        diagnostics.push_error(
            "Incorrect number of arguments for function '" + value.name +
                "'. Expected " + std::to_string(func->parameter_types.size()) +
                ", got " + std::to_string(args.size()) + ".",
            value.tokens.parenClose.source);
        return nullptr;
    }

    for (std::size_t i = 0; i < args.size(); i++) {
        auto* arg = args[i];

        std::string param_name;
        Source source{};
        if (func->definition != nullptr) {
            auto param_def = func->definition->parameters[i];
            param_name = param_def.name;
            source = node_source(value.arguments[i]);
        } else {
            param_name = "param" + std::to_string(i);
            source = value.tokens.identifier.source;
        }

        auto param_type = func->parameter_types[i];

        auto actual_type = scope.get_type_by_llvm(arg->getType());
        if (actual_type != param_type) {
            diagnostics.push_error(
                "Incorrect type for parameter '" + param_name + "'. Expected " +
                    param_type->name + ", got " + actual_type->name + ".",
                source);
            return nullptr;
        }
    }

    return builder.CreateCall(func->llvm, std::vector<llvm::Value*>{args});
}

auto compile_member_access(const MemberAccess& value, llvm::Module& module,
                           llvm::IRBuilder<>& builder, Scope& scope,
                           Diagnostics& diagnostics) -> llvm::Value* {
    auto* const base =
        compile_node(*value.base, module, builder, scope, diagnostics);
    if (base == nullptr) {
        return nullptr;
    }

    if (value.member->type != NodeType::identifier) {
        diagnostics.push_error("Member access must be an identifier",
                               value.tokens.dot.source);
        return nullptr;
    }

    const auto base_type = scope.get_type_by_llvm(base->getType());
    if (!base_type) {
        diagnostics.push_error("No type found for member access",
                               value.tokens.dot.source);
        return nullptr;
    }

    const auto identifier = std::get<Identifier>(value.member->value);

    const auto struct_index = get_member_index(
        *base_type, identifier.name, diagnostics, identifier.token.source);
    if (!struct_index.has_value()) {
        return nullptr;
    }

    const auto& member_type = scope.get_type(
        base_type->definition->members[struct_index.value()].type);
    if (!member_type) {
        diagnostics.push_error("No type found for member",
                               identifier.token.source);
        return nullptr;
    }

    llvm::Value* member_ptr =
        builder.CreateStructGEP(base->getType()->getPointerElementType(), base,
                                struct_index.value(), identifier.name + "_ptr");

    if (member_type->semantic == Type::Semantic::value) {
        return builder.CreateLoad(member_type->llvm, member_ptr);
    }

    return member_ptr;
}

auto compile_node(const Node& node, llvm::Module& module,
                  llvm::IRBuilder<>& builder, Scope& scope,
                  Diagnostics& diagnostics) -> llvm::Value* {
    std::cerr << "Compiling node " << node << '\n';
    switch (node.type) {
    case NodeType::function_definition: {
        return compile_function_definition(
            std::get<FunctionDefinition>(node.value), module, builder, scope,
            diagnostics);

    } break;
    case NodeType::identifier: {
        auto identifier = std::get<Identifier>(node.value);
        auto variable = scope.get_variable(identifier.name);
        if (!variable) {
            diagnostics.push_error("No variable named '" + identifier.name +
                                       "' found",
                                   identifier.token.source);
            return nullptr;
        }
        variable->uses += 1;
        return variable->llvm;
    } break;
    case NodeType::function_call: {
        return compile_function_call(std::get<FunctionCall>(node.value), module,
                                     builder, scope, diagnostics);
    } break;
    case NodeType::member_access: {
        return compile_member_access(std::get<MemberAccess>(node.value), module,
                                     builder, scope, diagnostics);
    } break;
    case NodeType::struct_definition: {
        const auto& value = std::get<StructDefinition>(node.value);
        auto llvm_members = std::vector<llvm::Type*>{};
        for (const auto& member : value.members) {
            auto type = scope.get_type(member.type);
            if (!type) {
                diagnostics.push_error("No type named " + member.type.name +
                                           " found",
                                       member.tokens.name.source);
                return nullptr;
            }
            llvm_members.push_back(type->llvm);
        }
        auto* llvm_value = llvm::StructType::create(
            module.getContext(), llvm_members, value.name, false);

        const auto type =
            Type{value.name, llvm_value, Type::Semantic::reference,
                 new StructDefinition{value.name, value.members, value.tokens}};
        scope.types[value.name] = std::make_shared<Type>(type);
    } break;
    case NodeType::string_literal: {
        auto value = std::get<StringLiteral>(node.value);
        auto string_type = scope.get_type_by_name("String");
        if (!string_type) {
            diagnostics.push_error("No type named String found",
                                   value.token.source);
            return nullptr;
        }

        auto* const alloca =
            builder.CreateAlloca(string_type->llvm, nullptr, "literal");

        builder.CreateStore(
            builder.CreateGlobalStringPtr(
                std::get<StringLiteral>(node.value).value),
            builder.CreateStructGEP(string_type->llvm, alloca, 0));

        return alloca;
    } break;
    case NodeType::integer_literal: {
        auto value = std::get<IntegerLiteral>(node.value);
        auto type = scope.get_type_by_name("Int");
        if (!type) {
            diagnostics.push_error("No type named Int found",
                                   value.token.source);
            return nullptr;
        }

        auto* const alloca =
            builder.CreateAlloca(type->llvm, nullptr, "literal");

        builder.CreateStore(
            llvm::ConstantInt::get(scope.get_type_by_name("Int64")->llvm,
                                   std::get<IntegerLiteral>(node.value).value,
                                   false),
            builder.CreateStructGEP(type->llvm, alloca, 0));

        return alloca;
    } break;
    case NodeType::variable_definition: {
        const auto& value = std::get<VariableDefinition>(node.value);
        if (scope.variables.find(value.name) != scope.variables.end()) {
            diagnostics.push_error("Variable '" + value.name +
                                       "' is already defined.",
                                   value.tokens.identifier.source);
            return nullptr;
        }

        auto* llvm =
            compile_node(*value.value, module, builder, scope, diagnostics);
        // TODO: get struct type from compile node somehow
        auto variable = Variable{value.name, scope.get_type_by_name("String"),
                                 llvm, value.tokens.identifier.source};
        scope.variables[value.name] = std::make_shared<Variable>(variable);
    } break;
    default: {
        std::cerr << "Unknown node type: " << node.type << '\n';
    } break;
    }

    return nullptr;
}

auto xlang::compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
    -> std::string {
    llvm::LLVMContext context;
    llvm::Module module("xlang", context);
    llvm::IRBuilder<> builder(context);

    auto scope = Scope{{}, {}, {}, nullptr};

    // scope.types["String"] =
    //     new Type{"String", builder.getInt8Ty()->getPointerTo()};
    scope.types["Int64"] =
        std::make_shared<Type>("Int64", builder.getInt64Ty());
    scope.types["Int32"] =
        std::make_shared<Type>("Int32", builder.getInt32Ty());
    scope.types["UInt8"] = std::make_shared<Type>("UInt8", builder.getInt8Ty());
    scope.types["Pointer<UInt8>"] = std::make_shared<Type>(
        "Pointer<UInt8>", builder.getInt8Ty()->getPointerTo(),
        Type::Semantic::value);
    scope.types["Void"] = std::make_shared<Type>("Void", builder.getVoidTy());

    while (!ast.empty()) {
        const auto node = ast.pop();
        compile_node(node, module, builder, scope, diagnostics);
    }

    warn_unused_variables(scope, diagnostics);

    std::string module_str;
    llvm::raw_string_ostream ostream(module_str);
    module.print(ostream, nullptr);
    return ostream.str();
}