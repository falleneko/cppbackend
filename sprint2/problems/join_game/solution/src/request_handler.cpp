#include "request_handler.h"
#include <cctype>
#include <regex>

namespace http_handler {
    using namespace std::literals;

    HandlerException::HandlerException(const std::string& msg)
        : message{msg} {
    }

    HandlerException::HandlerException() = default;

    HandlerException::HandlerException(const std::string& msg, const std::string& code,
                                       http::status status)
        : message{msg}
        , code{code}
        , status{status} {
    }

    const char* HandlerException::what() const noexcept {
        return message.c_str();
    }

    const std::string& HandlerException::GetCode() const noexcept {
        return code;
    }

    const http::status HandlerException::GetStatus() const noexcept {
        return status;
    }

    BadRequestException::BadRequestException()
        : HandlerException{"Bad request", "badRequest", http::status::bad_request} {
    }

    MapNotFoundException::MapNotFoundException()
        : HandlerException{"Map not found", "mapNotFound", http::status::not_found} {
    }

    NotFoundException::NotFoundException()
        : HandlerException{"Page not found", "pageNotFound", http::status::not_found} {
    }

    MethodNotAllowedException::MethodNotAllowedException()
        : HandlerException{"Invalid method", "invalidMethod", http::status::method_not_allowed} {
    }

    InvalidArgumentException::InvalidArgumentException(std::string message)
        : HandlerException{message, "invalidArgument", http::status::bad_request} {
    }

    InvalidTokenException::InvalidTokenException(std::string message)
        : HandlerException{message, "invalidToken", http::status::unauthorized} {
    }

    UnknownTokenException::UnknownTokenException()
        : HandlerException{"Player token has not been found", "unknownToken", http::status::unauthorized} {
    }

    RequestHandler::RequestHandler(model::Game& game, std::string static_dir)
        : static_dir_{std::move(static_dir)}
        , game_{game} {
    }

    std::string RequestHandler::API_URL = "/api/";

    const std::unordered_map<std::string, RequestHandler::ApiMethod> RequestHandler::api_methods_ = {
        {R"(v1\/maps$)", RequestHandler::ApiMethod::GET_MAPS},
        {R"(v1\/maps\/(.*[^\/])$)", RequestHandler::ApiMethod::GET_MAP},
        {R"(v1\/game\/join$)", RequestHandler::ApiMethod::JOIN_GAME},
        {R"(v1\/game\/players$)", RequestHandler::ApiMethod::GET_PLAYERS},
    };

    const std::unordered_map<RequestHandler::ApiMethod, RequestHandler::AllowedHttpMethods>
        RequestHandler::allowed_methods_ = {
            {RequestHandler::ApiMethod::GET_MAPS, {http::verb::get, http::verb::head}},
            {RequestHandler::ApiMethod::GET_MAP, {http::verb::get, http::verb::head}},
            {RequestHandler::ApiMethod::JOIN_GAME, {http::verb::post}},
            {RequestHandler::ApiMethod::GET_PLAYERS, {http::verb::get, http::verb::head}},
    };

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

    std::pair<RequestHandler::ApiMethod, std::string> RequestHandler::GetApiMethod(const std::string& url) {
        for (const auto& [pattern, method] : api_methods_) {
            std::smatch matches;
            if (std::regex_match(url, matches, std::regex(pattern))) {
                return {method, matches.size() > 1 ? matches[1].str() : std::string{}};
            }
        }
        return {ApiMethod::UNKNOWN, ""};
    }

    const RequestHandler::AllowedHttpMethods* RequestHandler::GetAllowedMethods(
        ApiMethod api_method) noexcept {
        if (const auto methods = allowed_methods_.find(api_method); methods != allowed_methods_.end()) {
            return &methods->second;
        }
        return nullptr;
    }

    std::string RequestHandler::MakeAllowHeader(const AllowedHttpMethods& methods) {
        std::string result;
        for (const http::verb method : methods) {
            if (!result.empty()) {
                result += ", ";
            }
            result += http::to_string(method);
        }
        return result;
    }

    bool RequestHandler::IsJsonContentType(std::string_view content_type) {
        const auto semicolon = content_type.find(';');
        content_type = content_type.substr(0, semicolon);
        while (!content_type.empty() && std::isspace(static_cast<unsigned char>(content_type.front()))) {
            content_type.remove_prefix(1);
        }
        while (!content_type.empty() && std::isspace(static_cast<unsigned char>(content_type.back()))) {
            content_type.remove_suffix(1);
        }
        std::string normalized{content_type};
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return normalized == "application/json";
    }

    model::Token RequestHandler::ParseAuthorization(std::string_view authorization) {
        constexpr std::string_view prefix = "Bearer ";
        constexpr size_t token_size = 32;
        if (!authorization.starts_with(prefix) || authorization.size() != prefix.size() + token_size) {
            throw InvalidTokenException("Authorization header is invalid");
        }
        const std::string_view token = authorization.substr(prefix.size());
        if (!std::all_of(token.begin(), token.end(), [](unsigned char ch) {
                return std::isxdigit(ch) != 0;
            })) {
            throw InvalidTokenException("Authorization header is invalid");
        }
        return model::Token{std::string{token}};
    }

    json::object RequestHandler::SerializeRoad(const model::Road& road) {
        json::object road_obj;
        if (road.IsHorizontal()) {
            road_obj.emplace("x1", road.GetEnd().x);
        } else if (road.IsVertical()) {
            road_obj.emplace("y1", road.GetEnd().y);
        }
        road_obj.emplace("x0", road.GetStart().x);
        road_obj.emplace("y0", road.GetStart().y);
        return road_obj;
    }

    json::object RequestHandler::SerializeBuilding(const model::Building& building) {
        json::object building_obj;
        building_obj.emplace("x", building.GetBounds().position.x);
        building_obj.emplace("y", building.GetBounds().position.y);
        building_obj.emplace("w", building.GetBounds().size.width);
        building_obj.emplace("h", building.GetBounds().size.height);
        return building_obj;
    }

    json::object RequestHandler::SerializeOffice(const model::Office& office) {
        json::object office_obj;
        office_obj.emplace("id", *office.GetId());
        office_obj.emplace("x", office.GetPosition().x);
        office_obj.emplace("y", office.GetPosition().y);
        office_obj.emplace("offsetX", office.GetOffset().dx);
        office_obj.emplace("offsetY", office.GetOffset().dy);
        return office_obj;
    }

    json::object RequestHandler::SerializeMap(const model::Map& map, const bool is_simple) {
        json::object res;
        res.emplace("id", *map.GetId());
        res.emplace("name", map.GetName());
        if (is_simple) {
            return res;
        }
        auto roads = map.GetRoads();
        res["roads"] = json::array();
        for (const auto& road : roads) {
            res["roads"]
                .as_array()
                .emplace_back(std::move(SerializeRoad(road)));
        }
        auto buildings = map.GetBuildings();
        res["buildings"] = json::array();
        for (const auto& building : buildings) {
            res["buildings"]
                .as_array()
                .emplace_back(std::move(SerializeBuilding(building)));
        }
        auto offices = map.GetOffices();
        res["offices"] = json::array();
        for (const auto& office : offices) {
            res["offices"]
                .as_array()
                .emplace_back(std::move(SerializeOffice(office)));
        }
        return res;
    }

    RequestHandler::HandlerResult RequestHandler::HandleGetMaps(ApiMethod method) {
        json::array res;
        const auto& maps = game_.GetMaps();
        for (const auto& map : maps) {
            res.emplace_back(SerializeMap(*map, true));
        }        
        return res;
    }

    RequestHandler::HandlerResult RequestHandler::HandleGetMap(ApiMethod method, std::string_view map_id) {
        const auto map = game_.FindMap(
            model::Map::Id{std::string{map_id}}
        );
        if (!map) {
            throw MapNotFoundException();
        }
        return SerializeMap(*map, false);
    }

    RequestHandler::HandlerResult RequestHandler::HandleJoinGame(std::string_view body) {
        std::string user_name;
        std::string map_id;
        try {
            const json::value parsed_request =
                json::parse(json::string_view{body.data(), body.size()});
            const json::object& request = parsed_request.as_object();
            user_name = json::value_to<std::string>(request.at("userName"));
            map_id = json::value_to<std::string>(request.at("mapId"));
        } catch (const std::exception&) {
            throw InvalidArgumentException("Join game request parse error");
        }

        if (user_name.empty()) {
            throw InvalidArgumentException("Invalid name");
        }

        const model::Map::Id requested_map_id{map_id};
        if (!game_.FindMap(requested_map_id)) {
            throw MapNotFoundException();
        }

        auto [player, token] = game_.JoinPlayer(std::move(user_name), requested_map_id);
        return json::object{
            {"authToken", *token},
            {"playerId", *player.GetId()}
        };
    }

    RequestHandler::HandlerResult RequestHandler::HandleGetPlayers(std::string_view authorization) {
        const model::Token token = ParseAuthorization(authorization);
        const model::Player* player = game_.FindPlayerByToken(token);
        if (!player) {
            throw UnknownTokenException();
        }

        json::object result;
        for (const model::Player* map_player : game_.GetPlayersOnMap(player->GetMap().GetId())) {
            result.emplace(std::to_string(*map_player->GetId()),
                           json::object{{"name", map_player->GetDog().GetName()}});
        }
        return result;
    }

    bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
        // Приводим оба пути к каноничному виду (без . и ..)
        path = fs::weakly_canonical(path);
        base = fs::weakly_canonical(base);

        // Проверяем, что все компоненты base содержатся внутри path
        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

}  // namespace http_handler
