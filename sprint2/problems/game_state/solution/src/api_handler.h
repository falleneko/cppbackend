#pragma once

#include "app.h"
#include "http_server.h"

#include <boost/json.hpp>

#include <algorithm>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::literals;

class HandlerException : public std::exception {
public:
    explicit HandlerException(const std::string& message);
    HandlerException();

    const char* what() const noexcept override;
    const std::string& GetCode() const noexcept;
    http::status GetStatus() const noexcept;

protected:
    HandlerException(std::string message, std::string code, http::status status);

private:
    std::string message_ = "Unknown error"s;
    std::string code_ = "unknownError"s;
    http::status status_ = http::status::internal_server_error;
};

class BadRequestException : public HandlerException {
public:
    BadRequestException();
};

class MapNotFoundException : public HandlerException {
public:
    MapNotFoundException();
};

class NotFoundException : public HandlerException {
public:
    NotFoundException();
};

class InvalidArgumentException : public HandlerException {
public:
    explicit InvalidArgumentException(std::string message = "Invalid argument");
};

class InvalidTokenException : public HandlerException {
public:
    explicit InvalidTokenException(std::string message);
};

class UnknownTokenException : public HandlerException {
public:
    UnknownTokenException();
};

class ApiHandler {
public:
    explicit ApiHandler(app::Application& application) noexcept;

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        http::request<Body, http::basic_fields<Allocator>>& req,
        Send& send,
        const std::string& url
    ) {
        try {
            HandleApi(req, send, url);
        } catch (const HandlerException& ex) {
            SendJsonResponse(
                req,
                send,
                json::object{{"code", ex.GetCode()}, {"message", ex.what()}},
                ex.GetStatus()
            );
        } catch (const std::exception& ex) {
            SendJsonResponse(
                req,
                send,
                json::object{{"code", "internalServerError"},
                {"message", ex.what()}},
                http::status::internal_server_error
            );
        }
    }

private:
    enum class ApiMethod {
        UNKNOWN,
        GET_MAP,
        GET_MAPS,
        JOIN_GAME,
        GET_PLAYERS,
        GET_GAME_STATE,
    };

    using HandlerResult = std::variant<json::object, json::array>;
    using AllowedHttpMethods = std::vector<http::verb>;

    template <typename Body, typename Allocator, typename Send>
    void SendJsonResponse(
        http::request<Body, http::basic_fields<Allocator>>& req,
        Send& send,
        const HandlerResult& body,
        http::status status,
        std::string_view allow = {}
    ) {
        std::string serialized;
        if (std::holds_alternative<json::object>(body)) {
            serialized = json::serialize(std::get<json::object>(body));
        } else {
            serialized = json::serialize(std::get<json::array>(body));
        }

        if (req.method() == http::verb::head) {
            http::response<http::empty_body> response{status, req.version()};
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            if (!allow.empty()) {
                response.set(http::field::allow, allow);
            }
            response.keep_alive(req.keep_alive());
            response.content_length(serialized.size());
            send(std::move(response));
            return;
        }

        http::response<http::string_body> response{status, req.version()};
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        if (!allow.empty()) {
            response.set(http::field::allow, allow);
        }
        response.keep_alive(req.keep_alive());
        response.body() = std::move(serialized);
        response.prepare_payload();
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleApi(http::request<Body, http::basic_fields<Allocator>>& req,
                   Send& send, const std::string& url) {
        const auto [api_method, target] = GetApiMethod(url);
        if (api_method == ApiMethod::UNKNOWN) {
            throw BadRequestException();
        }

        const AllowedHttpMethods* allowed_methods = GetAllowedMethods(api_method);
        if (!allowed_methods ||
            std::find(allowed_methods->begin(), allowed_methods->end(), req.method()) ==
                allowed_methods->end()) {
            SendJsonResponse(
                req, send,
                json::object{{"code", "invalidMethod"}, {"message", "Invalid method"}},
                http::status::method_not_allowed,
                allowed_methods ? MakeAllowHeader(*allowed_methods) : std::string{});
            return;
        }

        HandlerResult response_body;
        switch (api_method) {
            case ApiMethod::GET_MAPS:
                response_body = HandleGetMaps();
                break;
            case ApiMethod::GET_MAP:
                response_body = HandleGetMap(target);
                break;
            case ApiMethod::JOIN_GAME:
                if (!IsJsonContentType(std::string{req[http::field::content_type]})) {
                    throw InvalidArgumentException("Content-Type must be application/json");
                }
                {
                    std::lock_guard lock{application_mutex_};
                    response_body = HandleJoinGame(req.body());
                }
                break;
            case ApiMethod::GET_PLAYERS:
            case ApiMethod::GET_GAME_STATE: {
                const auto authorization = req.find(http::field::authorization);
                if (authorization == req.end()) {
                    throw InvalidTokenException("Authorization header is required");
                }
                const std::string_view value{authorization->value().data(),
                                             authorization->value().size()};
                std::lock_guard lock{application_mutex_};
                response_body = api_method == ApiMethod::GET_PLAYERS
                    ? HandleGetPlayers(value)
                    : HandleGetGameState(value);
                break;
            }
            case ApiMethod::UNKNOWN:
                throw BadRequestException();
        }

        SendJsonResponse(req, send, response_body, http::status::ok);
    }

    static std::pair<ApiMethod, std::string> GetApiMethod(const std::string& url);
    static const AllowedHttpMethods* GetAllowedMethods(ApiMethod method) noexcept;
    static std::string MakeAllowHeader(const AllowedHttpMethods& methods);
    static bool IsJsonContentType(std::string_view content_type);
    static app::Token ParseAuthorization(std::string_view authorization);

    HandlerResult HandleGetMaps() const;
    HandlerResult HandleGetMap(std::string_view map_id) const;
    HandlerResult HandleJoinGame(std::string_view body);
    HandlerResult HandleGetPlayers(std::string_view authorization) const;
    HandlerResult HandleGetGameState(std::string_view authorization) const;

    static json::object SerializeRoad(const model::Road& road);
    static json::object SerializeBuilding(const model::Building& building);
    static json::object SerializeOffice(const model::Office& office);
    static json::object SerializeMap(const model::Map& map, bool simple);
    static std::string_view SerializeDirection(model::Direction direction) noexcept;

    static const std::unordered_map<std::string, ApiMethod> api_methods_;
    static const std::unordered_map<ApiMethod, AllowedHttpMethods> allowed_methods_;

    app::Application& application_;
    std::mutex application_mutex_;
};

}  // namespace http_handler
