#include "core/ir/ir.h"
#include "core/parser/node.h"
#include "core/util/diagnostics.h"
#include <memory>
#include <optional>

using namespace xlang;
using namespace xlang::ir;

auto compile_type(const TypeIdentifier& type_identifier, Module& module,
                  Diagnostics& diagnostics) -> std::shared_ptr<Type> {
    if (module.types.contains(type_identifier.full_name())) {
        return module.types[type_identifier.full_name()];
    }

    std::shared_ptr<Type> type = nullptr;
    if (type_identifier.name == "Void") {
        type = std::make_shared<VoidType>(type_identifier);
    } else if (type_identifier.name == "Int64") {
        type = std::make_shared<PrimitiveType>(type_identifier, Primitive::i64);
    } else if (type_identifier.name == "Int32") {
        type = std::make_shared<PrimitiveType>(type_identifier, Primitive::i32);
    } else if (type_identifier.name == "UInt8") {
        type = std::make_shared<PrimitiveType>(type_identifier, Primitive::i8);
    } else if (type_identifier.name == "Pointer") {
        if (type_identifier.generic_parameters.size() != 1) {
            diagnostics.push_error(
                "Pointer type can only have one generic parameter, got " +
                    std::to_string(type_identifier.generic_parameters.size()),
                type_identifier.tokens.name.source);
            return compile_type(TypeIdentifier::_void(), module, diagnostics);
        }
        type = std::make_shared<PointerType>(
            type_identifier, compile_type(type_identifier.generic_parameters[0],
                                          module, diagnostics));
    }

    if (!type) {
        diagnostics.push_error("Unknown type: " + type_identifier.name,
                               type_identifier.tokens.name.source);
        return compile_type(TypeIdentifier::_void(), module, diagnostics);
    }

    module.types[type_identifier.full_name()] = type;
    return type;
}

auto compile_struct_definition(const StructDefinition& struct_definition,
                               Module& module, Diagnostics& diagnostics)
    -> std::shared_ptr<IRNode> {
    auto fields = std::unordered_map<std::string, std::shared_ptr<Type>>{};
    auto functions =
        std::unordered_map<std::string, std::shared_ptr<Function>>{};

    // TODO: rename to field
    for (const auto& member : struct_definition.members) {
        fields[member.name] = compile_type(member.type, module, diagnostics);
    }

    const auto struct_type_identifier =
        TypeIdentifier{struct_definition.name,
                       std::vector<TypeIdentifier>{},
                       {
                           struct_definition.tokens.identifier,
                           std::nullopt,
                           std::nullopt,
                       }};
    module.types[struct_type_identifier.full_name()] =
        std::make_shared<StructType>(struct_type_identifier, fields, functions);
    return nullptr;
}

auto compile_node(const Node& node, Module& module, Diagnostics& diagnostics)
    -> std::shared_ptr<IRNode>;

auto compile_function_definition(const FunctionDefinition& function_definition,
                                 Module& module, Diagnostics& diagnostics)
    -> std::shared_ptr<IRNode> {
    auto parameters = std::vector<Function::Parameter>{};
    auto body = std::vector<std::shared_ptr<IRNode>>{};

    for (const auto& parameter : function_definition.parameters) {
        parameters.emplace_back(
            parameter.name, compile_type(parameter.type, module, diagnostics));
    }

    const auto return_type_identifier =
        function_definition.return_type.value_or(
            TypeIdentifier{"Void",
                           {},
                           {function_definition.tokens.identifier, std::nullopt,
                            std::nullopt}});

    const auto return_type =
        compile_type(return_type_identifier, module, diagnostics);

    for (const auto& statement : function_definition.body) {
        body.push_back(compile_node(statement, module, diagnostics));
    }

    std::shared_ptr<IRNode> return_value = nullptr;
    if (function_definition.return_value) {
        return_value = compile_node(*function_definition.return_value, module,
                                    diagnostics);
    }

    if (return_value && return_value->type != return_type) {
        diagnostics.push_error("Function " + function_definition.name +
                                   " expects a return value of type " +
                                   return_type->identifier.full_name() +
                                   ", got " +
                                   return_value->type->identifier.full_name(),
                               function_definition.tokens.identifier.source);
        return nullptr;
    }

    if (return_value && return_type->identifier.name == "Void") {
        diagnostics.push_error("Function " + function_definition.name +
                                   " expects no return value, got one",
                               function_definition.tokens.identifier.source);
        return nullptr;
    }

    module.functions[function_definition.name] = std::make_shared<Function>(
        function_definition.name, function_definition, parameters, return_type,
        body, return_value);
    return nullptr;
}

auto compile_function_call(const FunctionCall& function_call, Module& module,
                           Diagnostics& diagnostics)
    -> std::shared_ptr<IRNode> {
    if (!module.functions.contains(function_call.name)) {
        diagnostics.push_error("Unknown function: " + function_call.name,
                               function_call.tokens.identifier.source);
        return nullptr;
    }

    const auto& function = module.functions[function_call.name];

    bool call_size_compatible = false;
    if (function->definition.variadic) {
        call_size_compatible =
            function_call.arguments.size() >= function->parameters.size();
    } else {
        call_size_compatible =
            function_call.arguments.size() == function->parameters.size();
    }

    if (!call_size_compatible) {
        diagnostics.push_error(
            "Function " + function_call.name + " expects " +
                std::to_string(function->parameters.size()) +
                " arguments, got " +
                std::to_string(function_call.arguments.size()),
            function_call.tokens.identifier.source);
        return nullptr;
    }

    std::vector<std::shared_ptr<IRNode>> arguments;

    for (size_t i = 0; i < function_call.arguments.size(); ++i) {
        const auto& argument =
            compile_node(function_call.arguments[i], module, diagnostics);
        if (!argument) {
            diagnostics.push_error("Function " + function_call.name +
                                       " argument " + std::to_string(i) +
                                       " could not be compiled",
                                   function_call.tokens.paren_open.source);
            return nullptr;
        }

        arguments.push_back(argument);
        if (i >= function->parameters.size()) {
            continue;
        }
        const auto& parameter = function->parameters[i];
        if (parameter.type != argument->type) {
            diagnostics.push_error(
                "Function " + function_call.name + " expects argument " +
                    std::to_string(i) + " to be of type " +
                    parameter.type->identifier.full_name() + ", got " +
                    argument->type->identifier.full_name(),
                function_call.tokens.identifier.source);
        }
    }

    return std::make_shared<FunctionCallIRNode>(function->return_type, function,
                                                arguments);
}

auto compile_node(const Node& node, Module& module, Diagnostics& diagnostics)
    -> std::shared_ptr<IRNode> {
    switch (node.type) {
    case NodeType::struct_definition:
        return compile_struct_definition(std::get<StructDefinition>(node.value),
                                         module, diagnostics);
    case NodeType::function_definition:
        return compile_function_definition(
            std::get<FunctionDefinition>(node.value), module, diagnostics);
    case NodeType::function_call:
        return compile_function_call(std::get<FunctionCall>(node.value), module,
                                     diagnostics);

    case NodeType::string_literal: {
        const auto& string_literal = std::get<StringLiteral>(node.value);
        return std::make_shared<StringLiteralIRNode>(
            string_literal,
            compile_type(TypeIdentifier::pointer_to(TypeIdentifier::uint8()),
                         module, diagnostics));
    }
    case NodeType::integer_literal: {
        const auto& integer_literal = std::get<IntegerLiteral>(node.value);
        return std::make_shared<IntegerLiteralIRNode>(
            integer_literal,
            compile_type(TypeIdentifier::int32(), module, diagnostics));
    }
    default:
        diagnostics.push_error("Unexpected node type: " +
                                   NodeType_to_string(node.type),
                               node_source(node));
        return nullptr;
    }
}

auto xlang::ir::compile(Buffer<std::vector<xlang::Node>> ast,
                        Diagnostics& diagnostics) -> Module {
    auto module = Module{};

    while (!ast.empty()) {
        const auto node = ast.pop();
        compile_node(node, module, diagnostics);
    }
    return module;
}