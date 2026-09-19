#pragma once
#include "http_server.h"
#include "logger.h"
#include "model.h"
#include <boost/json.hpp>
#include <chrono>
#include <unordered_map>
#include <variant>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace std::literals;

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& decorated)
        : decorated_{decorated} {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        const http_server::tcp::endpoint& endpoint,
        Send&& send
    ) {
        app_logging::Log("request received"sv, {
            {"ip", endpoint.address().to_string()},
            {"URI", std::string{req.target()}},
            {"method", std::string{req.method_string()}}
        });

        const auto started_at = std::chrono::steady_clock::now();
        decorated_(
            std::move(req),
            [endpoint, started_at, send = std::forward<Send>(send)](auto&& response) mutable {
                const auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();

                json::value content_type = nullptr;
                if (
                    const auto it = response.find(http::field::content_type);
                    it != response.end()
                ) {
                    content_type = std::string{it->value()};
                }

                app_logging::Log("response sent"sv, {
                    {"ip", endpoint.address().to_string()},
                    {"response_time", response_time},
                    {"code", response.result_int()},
                    {"content_type", std::move(content_type)}
                });
                send(std::forward<decltype(response)>(response));
            }
        );
    }

private:
    SomeRequestHandler& decorated_;
};

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

    RequestHandler(model::Game& game, std::string static_dir)
        : game_{game}
        , static_dir_{std::move(static_dir)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string url{req.target()};
        if (url.starts_with(API_URL)) {
            url = url.substr(API_URL.length(), url.length());
            try {
                HandleApi(req, send, url);
            } catch (const HandlerException& ex) {
                json::object body;
                body.emplace("code", ex.GetCode());
                body.emplace("message", ex.what());
                SendJsonResponse(req, send, body, ex.GetStatus());
            } catch (const std::exception& ex) {
                json::object body;
                body.emplace("code", "internalServerError");
                body.emplace("message", ex.what());
                SendJsonResponse(req, send, body, http::status::internal_server_error);
            }
        } else {
            try {
                HandleStatic(req, send, url);
            } catch (const HandlerException& ex) {
                SendStringResponse(req, send, ex.what(), ex.GetStatus());
            } catch (const std::exception& ex) {
                SendStringResponse(req, send, ex.what(), http::status::internal_server_error);
            }
        }
    }
private:
    using HandlerResult = std::variant<json::object, json::array>;
    using AllowedHttpMethods = std::tuple<RequestHandler::ApiMethod, http::verb>;

    // General

    template <typename ResponseBodyType, typename ResponseBody, typename RequestBody, typename Allocator, typename Send>
    void SendResponse(
        http::request<RequestBody, http::basic_fields<Allocator>>& req,
        Send& send,
        ResponseBody&& body,
        const http::status& status,
        const std::string& content_type
    ) {
        http::response<ResponseBodyType> res{status, req.version()};
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::move(body);
        res.prepare_payload();
        send(std::move(res));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendJsonResponse(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const HandlerResult& body, http::status status) {
        if (std::holds_alternative<json::object>(body)) {
            auto obj_b = json::serialize(std::get<json::object>(body));
            SendResponse<http::string_body>(req, send, obj_b, status, "application/json");
        } else if (std::holds_alternative<json::array>(body)) {
            auto obj_a = json::serialize(std::get<json::array>(body));
            SendResponse<http::string_body>(req, send, obj_a, status, "application/json");
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void SendStringResponse(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const std::string& body, http::status status) {
        SendResponse<http::string_body>(req, send, body, status, "text/plain");
    }

    template <typename Body, typename Allocator, typename Send>
    void SendFileResponse(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const std::string& file_path) {
        fs::path file_path_obj{file_path};

        if (fs::is_directory(file_path_obj)) {
            file_path_obj = file_path_obj / "index.html";
        }

        std::string detected_type{"application/octet-stream"};
        std::string file_ext{file_path_obj.extension().string()};
        if (mime_types_.contains(file_ext)) {
            detected_type = mime_types_.at(file_ext);
        }

        http::file_body::value_type file;        
        if (sys::error_code ec; file.open(file_path_obj.c_str(), beast::file_mode::read, ec), ec) {
            throw NotFoundException();
        }
        SendResponse<http::file_body>(req, send, file, http::status::ok, detected_type);
    }

    static std::tuple<RequestHandler::AllowedHttpMethods, std::string> GetApiMethod(const std::string& url);
    static std::string API_URL;

    static const std::unordered_map<std::string, AllowedHttpMethods> api_methods_;
    static const std::unordered_map<std::string, std::string> mime_types_;

    // Staticfiles

    template <typename Body, typename Allocator, typename Send>
    void HandleStatic(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const std::string& url) {
        if (req.method() != http::verb::get && req.method() != http::verb::post) {
            throw BadRequestException();
        }
        
        std::string current_req{static_dir_ + "/" + url};
        if (!IsSubPath(current_req, static_dir_)) {
            throw BadRequestException();
        }
        
        SendFileResponse(req, send, current_req);
    }

    static bool IsSubPath(fs::path path, fs::path base);

    // API

    template <typename Body, typename Allocator, typename Send>
    void HandleApi(http::request<Body, http::basic_fields<Allocator>>& req, Send& send, const std::string& url) {
        auto [allowed_methods, target] = GetApiMethod(url);
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

        SendJsonResponse(req, send, response_body, http::status::ok);
    }

    HandlerResult HandleGetMaps(ApiMethod method);
    HandlerResult HandleGetMap(ApiMethod method, std::string_view map_id);
    static json::object SerializeRoad(const model::Road& road);
    static json::object SerializeBuilding(const model::Building& building);
    static json::object SerializeOffice(const model::Office& office);
    static json::object SerializeMap(const model::Map& map, const bool simple);

    // Class fields

    std::string static_dir_;
    model::Game& game_;
};

}  // namespace http_handler
