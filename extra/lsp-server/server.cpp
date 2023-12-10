#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <optional>
#include <string>

using namespace boost;

auto serializeResponse(boost::json::object result, int id) -> std::string {
    boost::json::object message;
    message["jsonrpc"] = "2.0";
    message["id"] = id;
    message["result"] = result;

    // create an IO stream and serialize the JSON object to it
    auto json = boost::json::serialize(message);
    std::stringstream ss;
    ss << "Content-Length: " << json.size() << "\r\n\r\n";
    ss << json;
    return ss.str();
}

struct Request {
    int64_t id;
    std::string method;
};

auto parseRequest(std::string const& data) -> std::optional<Request> {
    std::size_t pos = data.find("{");
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto parsed = boost::json::parse(data.substr(pos));
    auto const& json = parsed.as_object();
    return Request{.id = json.contains("id") ? json.at("id").as_int64() : 0,
                   .method = json.at("method").as_string().c_str()};
}

auto main() -> int {
    try {
        asio::io_context io_context;
        asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "6000");

        asio::ip::tcp::socket socket(io_context);
        asio::connect(socket, endpoints);

        auto buffer = std::array<char, 2 << 12>{};
        while (true) {
            size_t len = socket.read_some(asio::buffer(buffer));
            if (!len)
                continue;
            const auto requestString = std::string(buffer.data(), len);
            std::cout << requestString << std::endl;

            const auto requestOpt = parseRequest(requestString);
            if (!requestOpt.has_value())
                continue;

            const auto request = requestOpt.value();

            auto response = std::optional<boost::json::object>{};
            if (request.method == "initialize") {
                response = boost::json::object{
                    {"serverInfo", boost::json::object{{"name", "xlang"}}},
                    {"capabilities",
                     boost::json::object{
                         {"textDocumentSync", 0},
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
                response = boost::json::object{
                    {"data",
                     boost::json::array{0, 0, 2, 0, 0, 0, 3, 4,  1, 0,
                                        1, 4, 5, 1, 1, 0, 6, 15, 2, 0}}};
            }

            if (response.has_value()) {
                const auto responseString =
                    serializeResponse(response.value(), request.id);
                std::cout << responseString << std::endl;
                asio::write(socket, asio::buffer(responseString));
            }
        }

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}