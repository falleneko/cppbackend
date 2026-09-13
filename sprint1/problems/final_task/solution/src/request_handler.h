#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <unordered_map>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

class HandlerException : public std::exception {
public:
    explicit HandlerException(const std::string& msg)
        : message{msg} {}

    HandlerException() = default;

    const char* what() const noexcept override {
        return message.c_str();
    }

    const std::string& GetCode() const noexcept {
        return code;
    }

    const http::status GetStatus() const noexcept {
        return status;
    }

protected:
    HandlerException(const std::string& msg, const std::string& code, http::status status)
        : message{msg}, code{code}, status{status} {}
    
private:
    std::string message = "Unknown error"s;
    std::string code = "unknownError"s;
    http::status status = http::status::internal_server_error;
};

class BadRequestException : public HandlerException {
public:
    BadRequestException() :
        HandlerException("Bad request", "badRequest", http::status::bad_request)
        {};
    using HandlerException::HandlerException;
};

class MapNotFoundException : public HandlerException {
public:
    MapNotFoundException() :
        HandlerException("Map not found", "mapNotFound", http::status::not_found)
        {};
    using HandlerException::HandlerException;
};

class NotFoundException : public HandlerException {
public:
    NotFoundException() :
        HandlerException("Page not found", "pageNotFound", http::status::not_found)
        {};
    using HandlerException::HandlerException;
};

class MethodNotAllowedException : public HandlerException {
public:
    MethodNotAllowedException() :
        HandlerException("Method not allowed", "methodNotAllowed", http::status::method_not_allowed)
        {};
    using HandlerException::HandlerException;
};

class RequestHandler {
public:
    enum class ApiMethod {
        UNKNOWN,
        GET_MAP,
        GET_MAPS
    };

    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        try {
            Run(req, send);
        } catch (const HandlerException& ex) {
            json::object body;
            body.emplace("code", ex.GetCode());
            body.emplace("message", ex.what());
            SendResponse(req, send, body, ex.GetStatus());
        } catch (const std::exception& ex) {
            json::object body;
            body.emplace("code", "internalServerError");
            body.emplace("message", ex.what());
            SendResponse(req, send, body, http::status::internal_server_error);
        }
    }
private:
    using HandlerResult = std::variant<json::object, json::array>;
    using AllowedHttpMethods = std::tuple<RequestHandler::ApiMethod, http::verb>;

    template <typename Body, typename Allocator, typename Send>
    void Run(http::request<Body, http::basic_fields<Allocator>>& req, Send& send) {
        std::string url(req.target());
        if (!url.starts_with(API_URL)) {
            HandleNotApi(req, send);
        }

        auto [allowed_methods, target] = GetApiMethod(std::move(url));
        auto [api_method, http_method] = allowed_methods;
        if (api_method == ApiMethod::UNKNOWN) {
            throw BadRequestException();
        }
        if (http_method != req.method()) {
            throw MethodNotAllowedException();
        }

        HandlerResult response_body;
        switch (api_method) {
            case ApiMethod::GET_MAPS:
                response_body = HandleGetMaps(api_method);
                break;
            case ApiMethod::GET_MAP:
                response_body = HandleGetMap(api_method, target);
                break;
            default:
                throw BadRequestException();
        }

        SendResponse(req, send, response_body);
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleNotApi(http::request<Body, http::basic_fields<Allocator>>& req, Send& send) {
        throw NotFoundException();
    }

    HandlerResult HandleGetMaps(ApiMethod method);
    HandlerResult HandleGetMap(ApiMethod method, std::string_view map_id);
    static json::object SerializeRoad(const model::Road& road);
    static json::object SerializeBuilding(const model::Building& building);
    static json::object SerializeOffice(const model::Office& office);
    static json::object SerializeMap(const model::Map& map, const bool simple);

    template <typename Body, typename Allocator, typename Send>
    void SendResponse(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const HandlerResult& body, http::status status = http::status::ok) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        if (std::holds_alternative<json::object>(body)) {
            res.body() = json::serialize(std::get<json::object>(body));
        } else if (std::holds_alternative<json::array>(body)) {
            res.body() = json::serialize(std::get<json::array>(body));
        }
        res.prepare_payload();
        send(std::move(res));
    }

    static std::tuple<RequestHandler::AllowedHttpMethods, std::string> GetApiMethod(const std::string url);
    static std::string API_URL;

    static const std::unordered_map<std::string, AllowedHttpMethods> api_methods_;
    model::Game& game_;
};

}  // namespace http_handler
