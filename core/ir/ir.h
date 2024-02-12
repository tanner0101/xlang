#pragma once

#include "core/parser/node.h"
#include "core/util/buffer.h"
#include "core/util/diagnostics.h"
#include "core/util/enum.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xlang::ir {

class Type {
  public:
    Type(const Type&) = default;
    Type(Type&&) = delete;
    auto operator=(const Type&) -> Type& = default;
    auto operator=(Type&&) -> Type& = delete;
    virtual ~Type() = default;

    Type(TypeIdentifier _identifier) : identifier{std::move(_identifier)} {}
    TypeIdentifier identifier;
};

class VoidType : public Type {
  public:
    VoidType(TypeIdentifier _identifier) : Type(std::move(_identifier)) {}
};

ENUM_CLASS(Primitive, u8, u16, u32, u64, i8, i16, i32, i64, f32, f64);

class PrimitiveType : public Type {
  public:
    PrimitiveType(TypeIdentifier _identifier, Primitive _primitive)
        : Type(std::move(_identifier)), primitive{_primitive} {}
    Primitive primitive;
};

class PointerType : public Type {
  public:
    PointerType(TypeIdentifier _identifier, std::shared_ptr<Type> pointee)
        : Type(std::move(_identifier)), pointee(std::move(pointee)) {}
    std::shared_ptr<Type> pointee;
};

class IRNode;

class Function {
  public:
    class Parameter {
      public:
        Parameter(std::string _name, std::shared_ptr<Type> _type)
            : name{std::move(_name)}, type{std::move(_type)} {}
        std::string name;
        std::shared_ptr<Type> type;
    };
    Function(std::string _name, FunctionDefinition _definition,
             std::vector<Parameter> _parameters,
             std::shared_ptr<Type> _return_type,
             std::vector<std::shared_ptr<IRNode>> _body,
             std::shared_ptr<IRNode> _return_value)
        : name{std::move(_name)}, definition{std::move(_definition)},
          parameters{std::move(_parameters)},
          return_type{std::move(_return_type)}, body{std::move(_body)},
          return_value{std::move(_return_value)} {}
    std::string name;
    FunctionDefinition definition;
    std::vector<Parameter> parameters;
    std::shared_ptr<Type> return_type;
    std::vector<std::shared_ptr<IRNode>> body;
    std::shared_ptr<IRNode> return_value;
};

class IRNode {
  public:
    IRNode(const IRNode&) = default;
    IRNode(IRNode&&) = delete;
    virtual ~IRNode() = default;
    IRNode(std::shared_ptr<Type> _type) : type{std::move(_type)} {}

    std::shared_ptr<Type> type;

    auto operator=(const IRNode&) -> IRNode& = default;
    auto operator=(IRNode&&) -> IRNode& = delete;
};

class FunctionCallIRNode : public IRNode {
  public:
    FunctionCallIRNode(std::shared_ptr<Type> _type,
                       std::shared_ptr<Function> _function,
                       std::vector<std::shared_ptr<IRNode>> _arguments)
        : IRNode{std::move(_type)}, function{std::move(_function)},
          arguments{std::move(_arguments)} {}
    std::shared_ptr<Function> function;
    std::vector<std::shared_ptr<IRNode>> arguments;
};

class StringLiteralIRNode : public IRNode {
  public:
    StringLiteralIRNode(StringLiteral _value, std::shared_ptr<Type> _type)
        : IRNode{std::move(_type)}, value{std::move(_value)} {}
    StringLiteral value;
};

class IntegerLiteralIRNode : public IRNode {
  public:
    IntegerLiteralIRNode(IntegerLiteral _value, std::shared_ptr<Type> _type)
        : IRNode{std::move(_type)}, value{std::move(_value)} {}
    IntegerLiteral value;
};

class StructType : public Type {
  public:
    StructType(
        TypeIdentifier _identifier,
        std::unordered_map<std::string, std::shared_ptr<Type>> _fields,
        std::unordered_map<std::string, std::shared_ptr<Function>> _functions)
        : Type(std::move(_identifier)), fields{std::move(_fields)},
          functions{std::move(_functions)} {}
    std::unordered_map<std::string, std::shared_ptr<Type>> fields;
    std::unordered_map<std::string, std::shared_ptr<Function>> functions;
};

class Module {
  public:
    std::unordered_map<std::string, std::shared_ptr<Type>> types;
    std::unordered_map<std::string, std::shared_ptr<Function>> functions;
};

inline auto operator<<(std::ostream& os, const Module& module)
    -> std::ostream& {
    os << "Module(types:";
    for (const auto& [name, type] : module.types) {
        os << name << ",";
    }
    os << ";functions:";
    for (const auto& [name, function] : module.functions) {
        os << name << ",";
    }
    os << ")";
    return os;
}

auto compile(Buffer<std::vector<xlang::Node>> ast, Diagnostics& diagnostics)
    -> Module;

} // namespace xlang::ir