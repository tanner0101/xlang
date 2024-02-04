#include "compiler.h"
#include "core/parser/node.h"
#include "core/util/diagnostics.h"
#include <cstddef>
#include <llvm-14/llvm/IR/Instructions.h>
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

    auto push_local() -> Scope { return Scope{{}, {}, {}, this}; }
};

auto compile_node(const Node& node, llvm::Module& module,
                  llvm::IRBuilder<>& builder, Scope& scope,
                  Diagnostics& diagnostics) -> std::optional<CompiledNode>;

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
                                 Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {

    if (scope.functions.find(value.name) != scope.functions.end()) {
        diagnostics.push_error("Function '" + value.name +
                                   "' is already defined.",
                               value.tokens.identifier.source);
        return std::nullopt;
    }

    std::vector<llvm::Type*> llvm_types{};
    std::vector<std::shared_ptr<Type>> types{};
    for (const auto& param : value.parameters) {
        auto type = scope.get_type(param.type);
        if (type == nullptr) {
            diagnostics.push_error("No type named " + param.type.name +
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
auto compile_type_init(const Type& type, const FunctionCall& value,
                       const std::vector<CompiledNode>& args,
                       llvm::Module& module, llvm::IRBuilder<>& builder,
                       Scope& scope, Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {

    auto* const alloca =
        builder.CreateAlloca(type.llvm, nullptr, type.name + "_init");

    if (value.arguments.size() != type.definition->members.size()) {
        diagnostics.push_error(
            "Incorrect number of arguments for type '" + type.name +
                "'. Expected " +
                std::to_string(type.definition->members.size()) + ", got " +
                std::to_string(value.arguments.size()) + ".",
            value.tokens.parenClose.source);
        return std::nullopt;
    }

    const auto source = value.tokens.identifier.source;

    for (std::size_t i = 0; i < value.arguments.size(); i++) {
        const auto& member = type.definition->members[i];
        auto const& arg = args[i];
        const auto member_type = scope.get_type(member.type);
        if (!member_type) {
            diagnostics.push_error("No type found for member", source);
            return std::nullopt;
        }

        if (arg.type != member_type) {
            diagnostics.push_error("Incorrect type for member '" + member.name +
                                       "'. Expected " + member_type->name +
                                       ", got " + arg.type->name + ".",
                                   source);
            return std::nullopt;
        }

        builder.CreateStore(builder.CreateLoad(arg.type->llvm, arg.llvm),
                            builder.CreateStructGEP(type.llvm, alloca, i));
    }

    // TODO: avoid scope get by name
    return CompiledNode{alloca, scope.get_type_by_name(type.name)};
}

auto compile_function_call(const FunctionCall& value, llvm::Module& module,
                           llvm::IRBuilder<>& builder, Scope& scope,
                           Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {

    std::vector<CompiledNode> args;
    for (const auto& arg : value.arguments) {
        auto const compiled_arg =
            compile_node(arg, module, builder, scope, diagnostics);
        if (!compiled_arg.has_value()) {
            return std::nullopt;
        }
        args.push_back(compiled_arg.value());
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
        return std::nullopt;
    }

    auto type = scope.get_type_by_name(value.name);

    if (args.size() != func->parameter_types.size()) {
        diagnostics.push_error(
            "Incorrect number of arguments for function '" + value.name +
                "'. Expected " + std::to_string(func->parameter_types.size()) +
                ", got " + std::to_string(args.size()) + ".",
            value.tokens.parenClose.source);
        return std::nullopt;
    }

    for (std::size_t i = 0; i < args.size(); i++) {
        auto arg = args[i];

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

        if (arg.type != param_type) {
            diagnostics.push_error(
                "Incorrect type for parameter '" + param_name + "'. Expected " +
                    param_type->name + ", got " + arg.type->name + ".",
                source);
            return std::nullopt;
        }
    }

    std::vector<llvm::Value*> llvm_args{};
    llvm_args.reserve(args.size());
    for (const auto& arg : args) {
        llvm_args.push_back(arg.llvm);
    }

    return CompiledNode{builder.CreateCall(func->llvm, llvm_args),
                        func->return_type};
}

auto compile_member_access(const MemberAccess& value, llvm::Module& module,
                           llvm::IRBuilder<>& builder, Scope& scope,
                           Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    auto const base =
        compile_node(*value.base, module, builder, scope, diagnostics);
    if (!base.has_value()) {
        return std::nullopt;
    }

    if (value.member->type != NodeType::identifier) {
        diagnostics.push_error("Member access must be an identifier",
                               value.tokens.dot.source);
        return std::nullopt;
    }

    const auto identifier = std::get<Identifier>(value.member->value);

    const auto struct_index =
        get_member_index(*base.value().type, identifier.name, diagnostics,
                         identifier.token.source);
    if (!struct_index.has_value()) {
        return std::nullopt;
    }

    const auto& member_type = scope.get_type(
        base.value().type->definition->members[struct_index.value()].type);
    if (!member_type) {
        diagnostics.push_error("No type found for member",
                               identifier.token.source);
        return std::nullopt;
    }

    llvm::Value* member_ptr =
        builder.CreateStructGEP(base.value().type->llvm, base->llvm,
                                struct_index.value(), identifier.name + "_ptr");

    if (member_type->semantic == Type::Semantic::value) {
        // TODO: should this become a different type?
        return CompiledNode{builder.CreateLoad(member_type->llvm, member_ptr),
                            member_type};
    }

    return CompiledNode{member_ptr, member_type};
}

auto compile_struct_definition(const StructDefinition& value,
                               llvm::Module& module, llvm::IRBuilder<>& builder,
                               Scope& scope, Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    if (scope.types.find(value.name) != scope.types.end()) {
        diagnostics.push_error("Type '" + value.name + "' is already defined.",
                               value.tokens.identifier.source);
        return std::nullopt;
    }

    auto llvm_members = std::vector<llvm::Type*>{};
    for (const auto& member : value.members) {
        auto type = scope.get_type(member.type);
        if (!type) {
            diagnostics.push_error("No type named " + member.type.name +
                                       " found",
                                   member.tokens.name.source);
            return std::nullopt;
        }
        llvm_members.push_back(type->llvm);
    }
    auto* llvm_value = llvm::StructType::create(
        module.getContext(), llvm_members, value.name, false);

    const auto type =
        Type{value.name, llvm_value, Type::Semantic::reference,
             new StructDefinition{value.name, value.members, value.tokens}};
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

auto compile_string_literal(const StringLiteral& value, llvm::Module& module,
                            llvm::IRBuilder<>& builder, Scope& scope,
                            Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    auto string_type = scope.get_type_by_name("String");
    if (!string_type) {
        diagnostics.push_error("No type named String found",
                               value.token.source);
        return std::nullopt;
    }

    auto* const alloca = builder.CreateAlloca(
        string_type->llvm, nullptr, "string_literal_" + safe_name(value.value));

    builder.CreateStore(builder.CreateGlobalStringPtr(value.value),
                        builder.CreateStructGEP(string_type->llvm, alloca, 0));

    return CompiledNode{alloca, string_type};
}

auto compile_integer_literal(const IntegerLiteral& value, llvm::Module& module,
                             llvm::IRBuilder<>& builder, Scope& scope,
                             Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    auto type = scope.get_type_by_name("Int");
    if (!type) {
        diagnostics.push_error("No type named Int found", value.token.source);
        return std::nullopt;
    }

    auto* const alloca = builder.CreateAlloca(
        type->llvm, nullptr, "int_literal_" + std::to_string(value.value));

    builder.CreateStore(
        llvm::ConstantInt::get(scope.get_type_by_name("Int64")->llvm,
                               value.value, false),
        builder.CreateStructGEP(type->llvm, alloca, 0));

    return CompiledNode{alloca, type};
}

auto compile_variable_definition(const VariableDefinition& value,
                                 llvm::Module& module,
                                 llvm::IRBuilder<>& builder, Scope& scope,
                                 Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    if (scope.variables.find(value.name) != scope.variables.end()) {
        diagnostics.push_error("Variable '" + value.name +
                                   "' is already defined.",
                               value.tokens.identifier.source);
        return std::nullopt;
    }

    auto rvalue =
        compile_node(*value.value, module, builder, scope, diagnostics);
    if (!rvalue.has_value()) {
        return std::nullopt;
    }

    auto variable =
        Variable{value.name, rvalue.value().type, rvalue.value().llvm,
                 value.tokens.identifier.source};
    scope.variables[value.name] = std::make_shared<Variable>(variable);

    return rvalue;
}

auto compile_identifier(const Identifier& value, llvm::Module& module,
                        llvm::IRBuilder<>& builder, Scope& scope,
                        Diagnostics& diagnostics)
    -> std::optional<CompiledNode> {
    auto variable = scope.get_variable(value.name);
    if (!variable) {
        diagnostics.push_error("No variable named '" + value.name + "' found",
                               value.token.source);
        return std::nullopt;
    }
    variable->uses += 1;
    return CompiledNode{variable->llvm, variable->type};
}

auto compile_node(const Node& node, llvm::Module& module,
                  llvm::IRBuilder<>& builder, Scope& scope,
                  Diagnostics& diagnostics) -> std::optional<CompiledNode> {
    std::cerr << "Compiling node " << node << '\n';
    switch (node.type) {
    case NodeType::function_definition:
        return compile_function_definition(
            std::get<FunctionDefinition>(node.value), module, builder, scope,
            diagnostics);

    case NodeType::identifier:
        return compile_identifier(std::get<Identifier>(node.value), module,
                                  builder, scope, diagnostics);
    case NodeType::function_call:
        return compile_function_call(std::get<FunctionCall>(node.value), module,
                                     builder, scope, diagnostics);
    case NodeType::member_access:
        return compile_member_access(std::get<MemberAccess>(node.value), module,
                                     builder, scope, diagnostics);
    case NodeType::struct_definition:
        return compile_struct_definition(std::get<StructDefinition>(node.value),
                                         module, builder, scope, diagnostics);
    case NodeType::string_literal:
        return compile_string_literal(std::get<StringLiteral>(node.value),
                                      module, builder, scope, diagnostics);
    case NodeType::integer_literal:
        return compile_integer_literal(std::get<IntegerLiteral>(node.value),
                                       module, builder, scope, diagnostics);
    case NodeType::variable_definition:
        return compile_variable_definition(
            std::get<VariableDefinition>(node.value), module, builder, scope,
            diagnostics);
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

    auto scope = Scope{{}, {}, {}, nullptr};

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