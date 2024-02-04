#include "core/compiler/compiler.h"
#include "core/lexer/lexer.h"
#include "core/parser/parser.h"
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <optional>
#include <string>

auto serialize_response(boost::json::object result, int64_t id) -> std::string {
    boost::json::object message;
    message["jsonrpc"] = "2.0";
    if (result.contains("params")) {
        message["method"] = result.at("method");
        message["params"] = result.at("params");
    } else {
        message["id"] = id;
        message["result"] = result;
    }

    auto json = boost::json::serialize(message);
    std::cerr << json << '\n';
    std::stringstream ss;
    ss << "Content-Length: " << json.size() << "\r\n\r\n";
    ss << json;
    return ss.str();
}

struct Request {
    int64_t id;
    std::string method;
    boost::json::object params;
};

auto parse_request(std::string const& data) -> std::optional<Request> {
    std::size_t pos = data.find('{');
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto parsed = boost::json::parse(data.substr(pos));
    auto const& json = parsed.as_object();
    return Request{.id = json.contains("id") ? json.at("id").as_int64() : 0,
                   .method = json.at("method").as_string().c_str(),
                   .params = json.at("params").as_object()};
}

auto convert_diagnostics(xlang::Diagnostics diagnostics) -> boost::json::array {
    auto result = boost::json::array{};
    for (const auto& diagnostic : diagnostics) {
        auto severity = 0;
        switch (diagnostic.type) {
        case xlang::DiagnosticType::error:
            severity = 1;
            break;
        case xlang::DiagnosticType::warning:
            severity = 2;
            break;
        case xlang::DiagnosticType::note:
            severity = 3;
            break;
        default:
            break;
        }
        result.push_back(boost::json::object{
            {"message", diagnostic.message},
            {"severity", severity},
            {"range",
             boost::json::object{
                 {"start",
                  boost::json::object{{"line", diagnostic.source.line},
                                      {"character", diagnostic.source.column}}},
                 {"end", boost::json::object{
                             {"line", diagnostic.source.line},
                             {"character", diagnostic.source.column}}}}}});
    }
    return result;
}

struct Context {
    std::unordered_map<std::string, std::string> files;
};

struct TextDocument {
    std::string uri;
    std::string data;
};

auto publish_diagnostics(const Context& ctx, const TextDocument file)
    -> boost::json::object {
    auto diagnostics = xlang::Diagnostics{};
    auto tokens = xlang::lex(file.data, diagnostics);
    auto ast = xlang::parse(tokens, diagnostics);
    compile(ast, diagnostics);
    return boost::json::object{
        {"method", "textDocument/publishDiagnostics"},
        {"params", boost::json::object{
                       {"uri", file.uri},
                       {"diagnostics", convert_diagnostics(diagnostics)}}}};
}

enum class SemanticTokenType {
    keyword = 0,
    function,
    string,
    number,
    variable,
    parameter,
    type
};
auto semantic_token_types() -> boost::json::array {
    return boost::json::array{"keyword",  "function",  "string", "number",
                              "variable", "parameter", "type"};
};

enum class SemanticTokenModifier : unsigned int {
    none = 0,
    declaration = 1 << 0,
    defaultLibrary = 1 << 1,
    _static = 1 << 2
};
inline auto operator|(SemanticTokenModifier lhs, SemanticTokenModifier rhs)
    -> SemanticTokenModifier {
    return static_cast<SemanticTokenModifier>(static_cast<unsigned int>(lhs) |
                                              static_cast<unsigned int>(rhs));
}
auto semantic_token_modifiers() -> boost::json::array {
    return boost::json::array{"declaration", "defaultLibrary", "static"};
};

auto semantic_token(const xlang::Token& token, std::size_t length,
                    SemanticTokenType type, SemanticTokenModifier modifier,
                    boost::json::array& data, xlang::Source& previous) -> void {
    int line_delta = token.source.line - previous.line;
    int column_delta = line_delta != 0 ? token.source.column
                                       : token.source.column - previous.column;

    previous.line = token.source.line;
    previous.column = token.source.column;

    data.push_back(line_delta);
    data.push_back(column_delta);
    data.push_back(length);
    data.push_back(static_cast<int>(type));
    data.push_back(static_cast<int>(modifier));
}

auto semantic_type(const xlang::TypeIdentifier& type, boost::json::array& data,
                   xlang::Source& previous) -> void {
    semantic_token(type.tokens.name, type.name.length(),
                   SemanticTokenType::type, SemanticTokenModifier::none, data,
                   previous);
    for (const auto& generic_parameter : type.genericParameters) {
        semantic_type(generic_parameter, data, previous);
    }
}

auto semantic_node(xlang::Node node, boost::json::array& data,
                   xlang::Source& previous) -> void {
    switch (node.type) {
    case xlang::NodeType::variable_definition: {
        auto value = std::get<xlang::VariableDefinition>(node.value);
        semantic_token(value.tokens.keyword, 3, SemanticTokenType::keyword,
                       SemanticTokenModifier::none, data, previous);
        semantic_token(
            value.tokens.identifier,
            std::get<std::string>(value.tokens.identifier.value).length(),
            SemanticTokenType::variable, SemanticTokenModifier::none, data,
            previous);
        semantic_node(*value.value, data, previous);
    } break;
    case xlang::NodeType::string_literal: {
        auto value = std::get<xlang::StringLiteral>(node.value);
        semantic_token(value.token, value.value.length() + 2,
                       SemanticTokenType::string, SemanticTokenModifier::none,
                       data, previous);
    } break;
    case xlang::NodeType::integer_literal: {
        auto value = std::get<xlang::IntegerLiteral>(node.value);
        semantic_token(value.token,
                       std::get<std::string>(value.token.value).length(),
                       SemanticTokenType::number, SemanticTokenModifier::none,
                       data, previous);
    } break;
    case xlang::NodeType::identifier: {
        auto value = std::get<xlang::Identifier>(node.value);
        semantic_token(value.token, value.name.length(),
                       SemanticTokenType::variable, SemanticTokenModifier::none,
                       data, previous);
    } break;
    case xlang::NodeType::struct_definition: {
        auto value = std::get<xlang::StructDefinition>(node.value);
        semantic_token(value.tokens.keyword, std::string("struct").length(),
                       SemanticTokenType::keyword, SemanticTokenModifier::none,
                       data, previous);
        semantic_token(
            value.tokens.identifier,
            std::get<std::string>(value.tokens.identifier.value).length(),
            SemanticTokenType::type, SemanticTokenModifier::declaration, data,
            previous);
        for (const auto& member : value.members) {
            semantic_token(member.tokens.name, member.name.length(),
                           SemanticTokenType::parameter,
                           SemanticTokenModifier::none, data, previous);
            semantic_type(member.type, data, previous);
        }
    } break;
    case xlang::NodeType::function_definition: {
        auto value = std::get<xlang::FunctionDefinition>(node.value);
        if (value.tokens.external.has_value()) {
            semantic_token(value.tokens.external.value(),
                           std::string("extern").length(),
                           SemanticTokenType::keyword,
                           SemanticTokenModifier::declaration, data, previous);
        }
        semantic_token(value.tokens.keyword, 2, SemanticTokenType::keyword,
                       SemanticTokenModifier::none, data, previous);
        semantic_token(
            value.tokens.identifier,
            std::get<std::string>(value.tokens.identifier.value).length(),
            SemanticTokenType::function, SemanticTokenModifier::none, data,
            previous);
        for (const auto& param : value.parameters) {
            semantic_token(param.tokens.identifier, param.name.length(),
                           SemanticTokenType::parameter,
                           SemanticTokenModifier::none, data, previous);
            semantic_type(param.type, data, previous);
        }
        if (value.return_type.has_value()) {
            semantic_type(value.return_type.value(), data, previous);
        }
        for (const auto& body : value.body) {
            semantic_node(body, data, previous);
        }
        if (value.tokens._return.has_value()) {
            semantic_token(value.tokens._return.value(),
                           std::string("return").length(),
                           SemanticTokenType::keyword,
                           SemanticTokenModifier::none, data, previous);
        }
        if (value.return_value != nullptr) {
            semantic_node(*value.return_value, data, previous);
        }
    } break;
    case xlang::NodeType::function_call: {
        auto funcal = std::get<xlang::FunctionCall>(node.value);
        semantic_token(
            funcal.tokens.identifier,
            std::get<std::string>(funcal.tokens.identifier.value).length(),
            SemanticTokenType::function, SemanticTokenModifier::none, data,
            previous);
        for (const auto& arg : funcal.arguments) {
            semantic_node(arg, data, previous);
        }
    } break;
    case xlang::NodeType::member_access: {
        auto value = std::get<xlang::MemberAccess>(node.value);
        semantic_node(*value.base, data, previous);
        semantic_node(*value.member, data, previous);
    } break;
    default:
        break;
    }
}

auto handle(Request request, Context& ctx)
    -> std::optional<boost::json::object> {
    if (request.method == "initialize") {
        return boost::json::object{
            {"serverInfo", boost::json::object{{"name", "xlang"}}},
            {"capabilities",
             boost::json::object{
                 {"textDocumentSync", 1},
                 {"semanticTokensProvider",
                  boost::json::object{
                      {"legend",
                       boost::json::object{
                           {"tokenTypes", semantic_token_types()},
                           {"tokenModifiers", semantic_token_modifiers()}}},
                      {"full", true}}}}}};
    }

    if (request.method == "textDocument/semanticTokens/full") {
        const auto* uri =
            request.params.at("textDocument").at("uri").as_string().c_str();

        auto data = boost::json::array{};
        auto diagnostics = xlang::Diagnostics{};
        auto tokens = xlang::lex(ctx.files[uri], diagnostics);
        auto ast = xlang::parse(tokens, diagnostics);

        auto previous = xlang::Source{};
        for (const auto& node : ast) {
            semantic_node(node, data, previous);
        }

        return boost::json::object{{"data", data}};
    }

    if (request.method == "textDocument/didOpen") {
        const auto text_document = request.params.at("textDocument");
        const auto* const uri = text_document.at("uri").as_string().c_str();
        const auto* const file = text_document.at("text").as_string().c_str();
        ctx.files[uri] = file;
        return publish_diagnostics(ctx, TextDocument{uri, file});
    }

    if (request.method == "textDocument/didChange") {
        const auto text_document = request.params.at("textDocument");
        const auto* const uri = text_document.at("uri").as_string().c_str();

        if (request.params.contains("contentChanges")) {
            const auto content_changes = request.params.at("contentChanges");
            const auto* const file =
                content_changes.at(0).at("text").as_string().c_str();
            ctx.files[uri] = file;
            return publish_diagnostics(ctx, TextDocument{uri, file});
        }
    }

    return std::nullopt;
}

auto main() -> int {
    auto ctx = Context{};
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "6000");

        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        constexpr auto buffer_size = 2 << 12;
        auto bytes = std::array<char, buffer_size>{};
        auto buffer = std::string{};
        while (true) {
            size_t len = socket.read_some(boost::asio::buffer(bytes));
            if (len == 0) {
                continue;
            }

            buffer += std::string(bytes.data(), len);
            size_t pos = 0;
            while ((pos = buffer.find("\r\n\r\n")) != std::string::npos) {
                // Extract and parse the Content-Length header
                static const std::string header = "Content-Length: ";
                size_t content_length_pos = buffer.find(header);
                if (content_length_pos == std::string::npos) {
                    break;
                }
                size_t start = content_length_pos + header.size();
                int content_length =
                    std::stoi(buffer.substr(start, pos - start));

                // Check if the complete message is in the buffer
                if (buffer.size() < pos + 4 + content_length) {
                    break; // Wait for more data
                }

                const auto request_string =
                    std::string(buffer.substr(pos + 4, content_length));
                std::cerr << request_string << '\n';
                const auto request_opt = parse_request(request_string);
                if (!request_opt.has_value()) {
                    continue;
                }

                const auto& request = request_opt.value();
                const auto response = handle(request, ctx);

                if (response.has_value()) {
                    const auto response_string =
                        serialize_response(response.value(), request.id);
                    boost::asio::write(socket,
                                       boost::asio::buffer(response_string));
                }

                // Remove the processed message from the buffer
                buffer.erase(0, pos + 4 + content_length);
            }
        }

    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    return 0;
}