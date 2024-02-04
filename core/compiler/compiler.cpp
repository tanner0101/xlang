#include "compiler.h"
#include "core/lexer/token.h"
#include "core/parser/node.h"
#include "core/util/diagnostics.h"
#include <cstddef>
#include <llvm-17/llvm/IR/Instructions.h>
#include <llvm-17/llvm/IR/LLVMContext.h>
#include <llvm-17/llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace xlang;

struct Type;

struct Function {
    std::string name;
    std::vector<std::shared_ptr<Type>> parameter_types;
    std::shared_ptr<Type> return_type;
    std::shared_ptr<FunctionDefinition> definition;
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
    std::shared_ptr<StructDefinition> definition;
};

struct CompiledNode {
    llvm::Value* llvm;
    std::shared_ptr<Type> type;
};

auto get_full_name(const TypeIdentifier& type) -> std::string {
    std::string name = type.name;
    for (const auto& param : type.genericParameters) {
        name += "<" + get_full_name(param) + ">";
    }
    return name;
}

struct Scope {
    std::unordered_map<std::string, std::shared_ptr<Variable>> variables = {};
    std::unordered_map<std::string, std::shared_ptr<Function>> functions = {};
    std::unordered_map<std::string, std::shared_ptr<Type>> types = {};

    Scope* parent = nullptr;

    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    Diagnostics& diagnostics;

    Scope(std::unordered_map<std::string, std::shared_ptr<Variable>> variables,
          std::unordered_map<std::string, std::shared_ptr<Function>> functions,
          std::unordered_map<std::string, std::shared_ptr<Type>> types,
          Scope* parent, llvm::Module& module, llvm::IRBuilder<>& builder,
          Diagnostics& diagnostics)
        : variables{std::move(variables)}, functions{std::move(functions)},
          types{std::move(types)}, parent{parent}, module{module},
          builder{builder}, diagnostics{diagnostics} {}

    ~Scope() = default;

    Scope(const Scope&) = delete;
    auto operator=(const Scope&) -> Scope& = delete;

    Scope(Scope&&) = delete;
    auto operator=(Scope&&) -> Scope& = delete;

    auto get_void_type() -> std::shared_ptr<Type> {
        return get_type_by_name("Void");
    }

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
        const auto name = get_full_name(type);
        if (types.find(name) != types.end()) {
            return types[name];
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

    auto push_local() -> Scope {
        return Scope{{}, {}, {}, this, module, builder, diagnostics};
    }
};

auto compile_node(const Node& node, Scope& scope)
    -> std::optional<CompiledNode>;

auto warn_unused_variables(const Scope& scope) -> void {
    for (const auto& variable : scope.variables) {
        if (variable.second->uses == 0) {
            scope.diagnostics.push_warning("Unused variable '" +
                                               variable.first + "'.",
                                           variable.second->source);
        }
    }
}

auto compile_function_definition(const FunctionDefinition& value, Scope& scope)
    -> std::optional<CompiledNode> {

    if (scope.functions.find(value.name) != scope.functions.end()) {
        scope.diagnostics.push_error("Function '" + value.name +
                                         "' is already defined.",
                                     value.tokens.identifier.source);
        return std::nullopt;
    }

    std::vector<llvm::Type*> llvm_types{};
    std::vector<std::shared_ptr<Type>> types{};
    for (const auto& param : value.parameters) {
        auto type = scope.get_type(param.type);
        if (type == nullptr) {
            scope.diagnostics.push_error("No type named " + param.type.name +
                                             " found",
                                         param.tokens.identifier.source);
            return std::nullopt;
        }
        if (type->definition != nullptr) {
            llvm_types.push_back(type->llvm->getPointerTo());
        } else {
            llvm_types.push_back(type->llvm);
        }
        types.push_back(type);
    }

    auto return_type = scope.get_void_type();
    if (value.return_type.has_value()) {
        return_type = scope.get_type(value.return_type.value());
        if (!return_type) {
            scope.diagnostics.push_error(
                "No type named " + value.return_type.value().name + " found",
                value.tokens.identifier.source);
            return std::nullopt;
        }
    }

    // TODO: support returning structs to stack
    auto* func = llvm::Function::Create(
        llvm::FunctionType::get(return_type->semantic == Type::Semantic::value
                                    ? return_type->llvm
                                    : return_type->llvm->getPointerTo(),
                                llvm_types, false),
        llvm::Function::ExternalLinkage, value.name, scope.module);

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
            llvm::BasicBlock::Create(scope.module.getContext(), "entry", func);
        scope.builder.SetInsertPoint(block);

        for (const auto& stmt : value.body) {
            compile_node(stmt, nested_scope);
        }

        if (return_type == scope.get_void_type()) {
            if (value.return_value != nullptr) {
                scope.diagnostics.push_error(
                    "Function '" + value.name +
                        "' has a return value but is declared as void.",
                    value.tokens.identifier.source);
                return std::nullopt;
            }
            scope.builder.CreateRetVoid();
        } else {
            if (!value.return_value) {
                scope.diagnostics.push_error("Function '" + value.name +
                                                 "' is missing a return value.",
                                             value.tokens.identifier.source);
                return std::nullopt;
            }

            auto return_value = compile_node(*value.return_value, nested_scope);
            if (!return_value.has_value()) {
                return std::nullopt;
            }

            if (return_value.value().type != return_type) {
                scope.diagnostics.push_error(
                    "Incorrect return type for function '" + value.name +
                        "'. Expected " + return_type->name + ", got " +
                        return_value.value().type->name + ".",
                    node_source(*value.return_value));
                return std::nullopt;
            }
            scope.builder.CreateRet(return_value.value().llvm);
        }
    }

    warn_unused_variables(nested_scope);

    auto function = Function{value.name, types, return_type,
                             std::make_shared<FunctionDefinition>(value), func};
    scope.functions[value.name] = std::make_shared<Function>(function);
    return std::nullopt;
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

auto compile_type_init(const std::shared_ptr<Type>& type,
                       const FunctionCall& value,
                       const std::vector<CompiledNode>& args, Scope& scope)
    -> std::optional<CompiledNode> {

    auto* const alloca =
        scope.builder.CreateAlloca(type->llvm, nullptr, type->name + "_init");

    if (value.arguments.size() != type->definition->members.size()) {
        scope.diagnostics.push_error(
            "Incorrect number of arguments for type '" + type->name +
                "'. Expected " +
                std::to_string(type->definition->members.size()) + ", got " +
                std::to_string(value.arguments.size()) + ".",
            value.tokens.parenClose.source);
        return std::nullopt;
    }

    const auto source = value.tokens.identifier.source;

    for (std::size_t i = 0; i < value.arguments.size(); i++) {
        const auto& member = type->definition->members[i];
        auto const& arg = args[i];
        const auto member_type = scope.get_type(member.type);
        if (!member_type) {
            scope.diagnostics.push_error("No type found for member", source);
            return std::nullopt;
        }

        if (arg.type != member_type) {
            scope.diagnostics.push_error(
                "Incorrect type for member '" + member.name + "'. Expected " +
                    member_type->name + ", got " + arg.type->name + ".",
                source);
            return std::nullopt;
        }

        scope.builder.CreateStore(
            scope.builder.CreateLoad(arg.type->llvm, arg.llvm),
            scope.builder.CreateStructGEP(type->llvm, alloca, i));
    }

    return CompiledNode{alloca, type};
}

auto compile_function_call(const FunctionCall& value, Scope& scope)
    -> std::optional<CompiledNode> {

    std::vector<CompiledNode> args;
    for (const auto& arg : value.arguments) {
        auto const compiled_arg = compile_node(arg, scope);
        if (!compiled_arg.has_value()) {
            return std::nullopt;
        }
        args.push_back(compiled_arg.value());
    }

    auto func = scope.get_function(value.name);
    if (!func) {
        const auto& type = scope.get_type_by_name(value.name);
        if (type) {
            return compile_type_init(type, value, args, scope);
        }

        scope.diagnostics.push_error("No function or type named '" +
                                         value.name + "' found.",
                                     value.tokens.identifier.source);
        return std::nullopt;
    }

    if (args.size() != func->parameter_types.size()) {
        scope.diagnostics.push_error(
            "Incorrect number of arguments for function '" + value.name +
                "'. Expected " + std::to_string(func->parameter_types.size()) +
                ", got " + std::to_string(args.size()) + ".",
            value.tokens.parenClose.source);
        return std::nullopt;
    }

    for (std::size_t i = 0; i < args.size(); i++) {

        if (!func->definition) {
            scope.diagnostics.push_error("No definition found for function",
                                         value.tokens.identifier.source);
            return std::nullopt;
        }

        auto arg = args[i];
        auto param_def = func->definition->parameters[i];
        auto param_type = func->parameter_types[i];

        if (arg.type != param_type) {
            scope.diagnostics.push_error("Incorrect type for parameter '" +
                                             param_def.name + "'. Expected " +
                                             param_type->name + ", got " +
                                             arg.type->name + ".",
                                         node_source(value.arguments[i]));
            return std::nullopt;
        }
    }

    std::vector<llvm::Value*> llvm_args{};
    llvm_args.reserve(args.size());
    for (const auto& arg : args) {
        llvm_args.push_back(arg.llvm);
    }

    return CompiledNode{scope.builder.CreateCall(func->llvm, llvm_args),
                        func->return_type};
}

auto compile_member_access(const MemberAccess& value, Scope& scope)
    -> std::optional<CompiledNode> {
    auto const base = compile_node(*value.base, scope);
    if (!base.has_value()) {
        return std::nullopt;
    }

    if (value.member->type != NodeType::identifier) {
        scope.diagnostics.push_error("Member access must be an identifier",
                                     value.tokens.dot.source);
        return std::nullopt;
    }

    const auto identifier = std::get<Identifier>(value.member->value);

    const auto struct_index =
        get_member_index(*base.value().type, identifier.name, scope.diagnostics,
                         identifier.token.source);
    if (!struct_index.has_value()) {
        return std::nullopt;
    }

    const auto& member_type = scope.get_type(
        base.value().type->definition->members[struct_index.value()].type);
    if (!member_type) {
        scope.diagnostics.push_error("No type found for member",
                                     identifier.token.source);
        return std::nullopt;
    }

    llvm::Value* member_ptr = scope.builder.CreateStructGEP(
        base.value().type->llvm, base->llvm, struct_index.value(),
        identifier.name + "_ptr");

    if (member_type->semantic == Type::Semantic::value) {
        // TODO: should this become a different type?
        return CompiledNode{
            scope.builder.CreateLoad(member_type->llvm, member_ptr),
            member_type};
    }

    return CompiledNode{member_ptr, member_type};
}

auto compile_struct_definition(const StructDefinition& value, Scope& scope)
    -> std::optional<CompiledNode> {
    if (scope.types.find(value.name) != scope.types.end()) {
        scope.diagnostics.push_error("Type '" + value.name +
                                         "' is already defined.",
                                     value.tokens.identifier.source);
        return std::nullopt;
    }

    auto llvm_members = std::vector<llvm::Type*>{};
    for (const auto& member : value.members) {
        auto type = scope.get_type(member.type);
        if (!type) {
            scope.diagnostics.push_error("No type named " + member.type.name +
                                             " found",
                                         member.tokens.name.source);
            return std::nullopt;
        }
        llvm_members.push_back(type->llvm);
    }
    auto* llvm_value = llvm::StructType::create(
        scope.module.getContext(), llvm_members, value.name, false);

    const auto type = Type{value.name, llvm_value, Type::Semantic::reference,
                           std::make_shared<StructDefinition>(value)};
    scope.types[value.name] = std::make_shared<Type>(type);
    return std::nullopt;
}

auto safe_name(const std::string& input) -> std::string {
    std::string safe_name;
    for (char c : input) {
        if (std::isalnum(c) != 0 || c == '_') {
            safe_name += c;
        } else if (std::isspace(c) != 0) {
            safe_name += '_';
        }
    }
    return safe_name;
}

auto compile_string_literal(const StringLiteral& value, Scope& scope)
    -> std::optional<CompiledNode> {
    auto string_type = scope.get_type_by_name("String");
    if (!string_type) {
        scope.diagnostics.push_error("No type named String found",
                                     value.token.source);
        return std::nullopt;
    }

    auto* const alloca = scope.builder.CreateAlloca(
        string_type->llvm, nullptr, "string_literal_" + safe_name(value.value));

    scope.builder.CreateStore(
        scope.builder.CreateGlobalStringPtr(value.value),
        scope.builder.CreateStructGEP(string_type->llvm, alloca, 0));

    return CompiledNode{alloca, string_type};
}

auto compile_integer_literal(const IntegerLiteral& value, Scope& scope)
    -> std::optional<CompiledNode> {
    auto type = scope.get_type_by_name("Int");
    if (!type) {
        scope.diagnostics.push_error("No type named Int found",
                                     value.token.source);
        return std::nullopt;
    }

    auto* const alloca = scope.builder.CreateAlloca(
        type->llvm, nullptr, "int_literal_" + std::to_string(value.value));

    scope.builder.CreateStore(
        llvm::ConstantInt::get(scope.get_type_by_name("Int64")->llvm,
                               value.value, false),
        scope.builder.CreateStructGEP(type->llvm, alloca, 0));

    return CompiledNode{alloca, type};
}

auto compile_variable_definition(const VariableDefinition& value, Scope& scope)
    -> std::optional<CompiledNode> {
    if (scope.variables.find(value.name) != scope.variables.end()) {
        scope.diagnostics.push_error("Variable '" + value.name +
                                         "' is already defined.",
                                     value.tokens.identifier.source);
        return std::nullopt;
    }

    auto rvalue = compile_node(*value.value, scope);
    if (!rvalue.has_value()) {
        return std::nullopt;
    }

    auto variable =
        Variable{value.name, rvalue.value().type, rvalue.value().llvm,
                 value.tokens.identifier.source};
    scope.variables[value.name] = std::make_shared<Variable>(variable);

    return rvalue;
}

auto compile_identifier(const Identifier& value, Scope& scope)
    -> std::optional<CompiledNode> {
    auto variable = scope.get_variable(value.name);
    if (!variable) {
        scope.diagnostics.push_error(
            "No variable named '" + value.name + "' found", value.token.source);
        return std::nullopt;
    }
    variable->uses += 1;
    return CompiledNode{variable->llvm, variable->type};
}

auto compile_node(const Node& node, Scope& scope)
    -> std::optional<CompiledNode> {
    std::cerr << "Compiling node " << node << '\n';
    switch (node.type) {
    case NodeType::function_definition:
        return compile_function_definition(
            std::get<FunctionDefinition>(node.value), scope);

    case NodeType::identifier:
        return compile_identifier(std::get<Identifier>(node.value), scope);
    case NodeType::function_call:
        return compile_function_call(std::get<FunctionCall>(node.value), scope);
    case NodeType::member_access:
        return compile_member_access(std::get<MemberAccess>(node.value), scope);
    case NodeType::struct_definition:
        return compile_struct_definition(std::get<StructDefinition>(node.value),
                                         scope);
    case NodeType::string_literal:
        return compile_string_literal(std::get<StringLiteral>(node.value),
                                      scope);
    case NodeType::integer_literal:
        return compile_integer_literal(std::get<IntegerLiteral>(node.value),
                                       scope);
    case NodeType::variable_definition:
        return compile_variable_definition(
            std::get<VariableDefinition>(node.value), scope);
    default: {
        std::cerr << "Unknown node type: " << node.type << '\n';
    } break;
    }

    return std::nullopt;
}

auto xlang::compile(Buffer<std::vector<Node>> ast, Diagnostics& diagnostics)
    -> std::string {
    llvm::LLVMContext context;
    llvm::Module module("xlang", context);
    llvm::IRBuilder<> builder(context);

    auto scope = Scope{{}, {}, {}, nullptr, module, builder, diagnostics};

    scope.types["Int64"] =
        std::make_shared<Type>("Int64", builder.getInt64Ty());
    scope.types["Int32"] =
        std::make_shared<Type>("Int32", builder.getInt32Ty());
    scope.types["UInt8"] = std::make_shared<Type>("UInt8", builder.getInt8Ty());
    scope.types["Pointer<UInt8>"] = std::make_shared<Type>(
        "Pointer<UInt8>", builder.getInt8Ty()->getPointerTo(),
        Type::Semantic::value);
    scope.types["Void"] = std::make_shared<Type>("Void", builder.getVoidTy(),
                                                 Type::Semantic::value);

    while (!ast.empty()) {
        const auto node = ast.pop();
        compile_node(node, scope);
    }

    warn_unused_variables(scope);

    std::string module_str;
    llvm::raw_string_ostream ostream(module_str);
    module.print(ostream, nullptr);
    return ostream.str();
}