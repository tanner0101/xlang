#include "core/compiler/compiler.h"
#include "core/lexer/lexer.h"
#include "core/parser/parser.h"
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

auto serializeResponse(boost::json::object result, int id) -> std::string {
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
    std::cerr << json << std::endl;
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

auto parseRequest(std::string const& data) -> std::optional<Request> {
    std::size_t pos = data.find("{");
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto parsed = boost::json::parse(data.substr(pos));
    auto const& json = parsed.as_object();
    return Request{.id = json.contains("id") ? json.at("id").as_int64() : 0,
                   .method = json.at("method").as_string().c_str(),
                   .params = json.at("params").as_object()};
}

auto convertDiagnostics(xlang::Diagnostics diagnostics) -> boost::json::array {
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
    xlang::Parser parser;
    xlang::Lexer lexer;
    xlang::Compiler compiler;
    std::unordered_map<std::string, std::string> files;
};

auto publishDiagnostics(Context ctx, std::string uri, std::string file)
    -> boost::json::object {
    auto diagnostics = xlang::Diagnostics{};
    auto tokens = ctx.lexer.lex(file, diagnostics);
    auto ast = ctx.parser.parse(tokens, diagnostics);
    ctx.compiler.compile(ast, diagnostics);
    return boost::json::object{
        {"method", "textDocument/publishDiagnostics"},
        {"params",
         boost::json::object{
             {"uri", uri}, {"diagnostics", convertDiagnostics(diagnostics)}}}};
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
                           {"tokenTypes",
                            boost::json::array{"keyword", "function",
                                               "string"}},
                           {"tokenModifiers",
                            boost::json::array{"defaultLibrary"}}}},
                      {"full", true}}}}}};
    } else if (request.method == "textDocument/semanticTokens/full") {
        auto uri =
            request.params.at("textDocument").at("uri").as_string().c_str();

        auto data = boost::json::array{};
        auto diagnostics = xlang::Diagnostics{};
        const auto tokens = ctx.lexer.lex(ctx.files[uri], diagnostics);

        int previousLine = 0;
        int previousColumn = 0;

        for (const auto& token : tokens) {
            int lineDelta = token.source.line - previousLine;
            int columnDelta = lineDelta ? token.source.column
                                        : token.source.column - previousColumn;

            int length = 0;
            int type = 0;
            int modifier = 0;

            switch (token.type) {
            case xlang::TokenType::function:
                length = 2;
                break;
            case xlang::TokenType::identifier:
                length = std::get<std::string>(token.value).size();
                type = 1;
                break;
            case xlang::TokenType::string_literal:
                length = std::get<std::string>(token.value).size() + 2;
                type = 2;
                modifier = 1;
                break;
            default:
                continue;
            }
            previousLine = token.source.line;
            previousColumn = token.source.column;

            data.push_back(lineDelta);
            data.push_back(columnDelta);
            data.push_back(length);
            data.push_back(type);
            data.push_back(modifier);
        }

        return boost::json::object{{"data", data}};
    } else if (request.method == "textDocument/didOpen") {
        const auto textDocument = request.params.at("textDocument");
        const auto uri = textDocument.at("uri").as_string().c_str();
        const auto file = textDocument.at("text").as_string().c_str();
        ctx.files[uri] = file;
        return publishDiagnostics(ctx, uri, file);
    } else if (request.method == "textDocument/didChange") {
        const auto textDocument = request.params.at("textDocument");
        const auto uri = textDocument.at("uri").as_string().c_str();

        if (request.params.contains("contentChanges")) {
            const auto contentChanges = request.params.at("contentChanges");
            const auto file =
                contentChanges.at(0).at("text").as_string().c_str();
            ctx.files[uri] = file;
            return publishDiagnostics(ctx, uri, file);
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

        auto bytes = std::array<char, 2 << 12>{};
        auto buffer = std::string{};
        while (true) {
            size_t len = socket.read_some(boost::asio::buffer(bytes));
            if (!len)
                continue;
            buffer += std::string(bytes.data(), len);
            size_t pos = 0;
            while ((pos = buffer.find("\r\n\r\n")) != std::string::npos) {
                // Extract and parse the Content-Length header
                static const std::string header = "Content-Length: ";
                size_t contentLengthPos = buffer.find(header);
                if (contentLengthPos == std::string::npos) {
                    break;
                }
                size_t start = contentLengthPos + header.size();
                int contentLength =
                    std::stoi(buffer.substr(start, pos - start));

                // Check if the complete message is in the buffer
                if (buffer.size() < pos + 4 + contentLength) {
                    break; // Wait for more data
                }

                const auto requestString =
                    std::string(buffer.substr(pos + 4, contentLength));
                std::cerr << requestString << std::endl;
                const auto requestOpt = parseRequest(requestString);
                if (!requestOpt.has_value())
                    continue;

                const auto request = requestOpt.value();
                const auto response = handle(request, ctx);

                if (response.has_value()) {
                    const auto responseString =
                        serializeResponse(response.value(), request.id);
                    boost::asio::write(socket,
                                       boost::asio::buffer(responseString));
                }

                // Remove the processed message from the buffer
                buffer.erase(0, pos + 4 + contentLength);
            }
        }

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}