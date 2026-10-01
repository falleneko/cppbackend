#include "request_handler.h"

#include <utility>

namespace http_handler {

RequestHandler::RequestHandler(ApiHandler& api_handler, std::string static_dir,
                               net::io_context& ioc)
    : RequestHandler{api_handler, std::move(static_dir), net::make_strand(ioc)} {
}

RequestHandler::RequestHandler(ApiHandler& api_handler, std::string static_dir,
                               Strand api_strand)
    : api_handler_{api_handler}
    , static_dir_{std::move(static_dir)}
    , api_strand_{std::move(api_strand)} {
}

const std::unordered_map<std::string, std::string> RequestHandler::mime_types_ = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".css", "text/css"},
    {".txt", "text/plain"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpe", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".ico", "image/vnd.microsoft.icon"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},
    {".svg", "image/svg+xml"},
    {".svgz", "image/svg+xml"},
    {".mp3", "audio/mpeg"},
};

bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    auto path_it = path.begin();
    auto base_it = base.begin();
    while (base_it != base.end()) {
        if (path_it == path.end() || *path_it != *base_it) {
            return false;
        }
        ++base_it;
        ++path_it;
    }
    return true;
}

}  // namespace http_handler
