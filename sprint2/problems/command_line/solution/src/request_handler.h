#pragma once

#include "api_handler.h"
#include "http_server.h"
#include "logger.h"

#include <boost/json.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace http_handler {

namespace beast = boost::beast;
namespace net = boost::asio;
namespace fs = std::filesystem;
namespace http = beast::http;
namespace json = boost::json;
namespace sys = boost::system;

using namespace std::literals;

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& decorated)
        : decorated_{decorated} {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const http_server::tcp::endpoint& endpoint, Send&& send) {
        app_logging::Log("request received"sv,
                         {{"ip", endpoint.address().to_string()},
                          {"URI", std::string{req.target()}},
                          {"method", std::string{req.method_string()}}});

        const auto started_at = std::chrono::steady_clock::now();
        decorated_(
            std::move(req),
            [endpoint, started_at, send = std::forward<Send>(send)](
                auto&& response) mutable {
                const auto response_time =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started_at)
                        .count();

                json::value content_type = nullptr;
                if (const auto it = response.find(http::field::content_type);
                    it != response.end()) {
                    content_type = std::string{it->value()};
                }

                app_logging::Log("response sent"sv,
                                 {{"ip", endpoint.address().to_string()},
                                  {"response_time", response_time},
                                  {"code", response.result_int()},
                                  {"content_type", std::move(content_type)}});
                send(std::forward<decltype(response)>(response));
            });
    }

private:
    SomeRequestHandler& decorated_;
};

class RequestHandler {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    RequestHandler(ApiHandler& api_handler, std::string static_dir,
                   net::io_context& ioc);
    RequestHandler(ApiHandler& api_handler, std::string static_dir,
                   Strand api_strand);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
        const std::string url{req.target()};
        if (url.starts_with(API_URL)) {
            net::dispatch(
                api_strand_,
                [this, req = std::move(req),
                 send = std::forward<Send>(send),
                 target = url.substr(API_URL.size())]() mutable {
                    api_handler_(req, send, target);
                });
            return;
        }

        try {
            HandleStatic(req, send, url);
        } catch (const HandlerException& ex) {
            SendStringResponse(req, send, ex.what(), ex.GetStatus());
        } catch (const std::exception& ex) {
            SendStringResponse(req, send, ex.what(),
                               http::status::internal_server_error);
        }
    }

private:
    template <typename ResponseBodyType, typename ResponseBody,
              typename RequestBody, typename Allocator, typename Send>
    void SendResponse(http::request<RequestBody, http::basic_fields<Allocator>>& req,
                      Send& send, ResponseBody&& body, http::status status,
                      const std::string& content_type) {
        http::response<ResponseBodyType> response{status, req.version()};
        response.set(http::field::content_type, content_type);
        response.keep_alive(req.keep_alive());
        response.body() = std::forward<ResponseBody>(body);
        response.prepare_payload();
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendStringResponse(http::request<Body, http::basic_fields<Allocator>>& req,
                            Send& send, const std::string& body,
                            http::status status) {
        SendResponse<http::string_body>(req, send, std::string{body}, status,
                                        "text/plain");
    }

    template <typename Body, typename Allocator, typename Send>
    void SendFileResponse(http::request<Body, http::basic_fields<Allocator>>& req,
                          Send& send, const std::string& file_path) {
        fs::path path{file_path};
        if (fs::is_directory(path)) {
            path /= "index.html";
        }

        std::string content_type{"application/octet-stream"};
        if (const auto it = mime_types_.find(path.extension().string());
            it != mime_types_.end()) {
            content_type = it->second;
        }

        http::file_body::value_type file;
        if (sys::error_code error; file.open(path.c_str(), beast::file_mode::read, error),
            error) {
            throw NotFoundException();
        }
        SendResponse<http::file_body>(req, send, std::move(file), http::status::ok,
                                      content_type);
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleStatic(http::request<Body, http::basic_fields<Allocator>>& req,
                      Send& send, const std::string& url) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            throw BadRequestException();
        }

        const fs::path requested_path = fs::path{static_dir_} / url.substr(1);
        if (!IsSubPath(requested_path, static_dir_)) {
            throw BadRequestException();
        }
        SendFileResponse(req, send, requested_path.string());
    }

    static bool IsSubPath(fs::path path, fs::path base);

    inline static constexpr std::string_view API_URL = "/api/";
    static const std::unordered_map<std::string, std::string> mime_types_;

    ApiHandler& api_handler_;
    std::string static_dir_;
    Strand api_strand_;
};

}  // namespace http_handler
