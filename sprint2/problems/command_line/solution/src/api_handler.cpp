#include "api_handler.h"

#include <cctype>
#include <chrono>
#include <regex>
#include <utility>

namespace http_handler {

HandlerException::HandlerException(const std::string& message)
    : message_{message} {
}

HandlerException::HandlerException() = default;

HandlerException::HandlerException(std::string message, std::string code,
                                   http::status status)
    : message_{std::move(message)}
    , code_{std::move(code)}
    , status_{status} {
}

const char* HandlerException::what() const noexcept {
    return message_.c_str();
}

const std::string& HandlerException::GetCode() const noexcept {
    return code_;
}

http::status HandlerException::GetStatus() const noexcept {
    return status_;
}

BadRequestException::BadRequestException()
    : HandlerException{"Invalid endpoint", "badRequest", http::status::bad_request} {
}

MapNotFoundException::MapNotFoundException()
    : HandlerException{"Map not found", "mapNotFound", http::status::not_found} {
}

NotFoundException::NotFoundException()
    : HandlerException{"Page not found", "pageNotFound", http::status::not_found} {
}

InvalidArgumentException::InvalidArgumentException(std::string message)
    : HandlerException{std::move(message), "invalidArgument", http::status::bad_request} {
}

InvalidTokenException::InvalidTokenException(std::string message)
    : HandlerException{std::move(message), "invalidToken", http::status::unauthorized} {
}

UnknownTokenException::UnknownTokenException()
    : HandlerException{"Player token has not been found", "unknownToken",
                       http::status::unauthorized} {
}

ApiHandler::ApiHandler(app::Application& application,
                       bool manual_time_control) noexcept
    : application_{application}
    , manual_time_control_{manual_time_control} {
}

const std::unordered_map<std::string, ApiHandler::ApiMethod> ApiHandler::api_methods_ = {
    {R"(v1\/maps$)", ApiMethod::GET_MAPS},
    {R"(v1\/maps\/(.*[^\/])$)", ApiMethod::GET_MAP},
    {R"(v1\/game\/join$)", ApiMethod::JOIN_GAME},
    {R"(v1\/game\/players$)", ApiMethod::GET_PLAYERS},
    {R"(v1\/game\/state$)", ApiMethod::GET_GAME_STATE},
    {R"(v1\/game\/player\/action$)", ApiMethod::SET_PLAYER_ACTION},
    {R"(v1\/game\/tick$)", ApiMethod::TICK},
};

const std::unordered_map<ApiHandler::ApiMethod, ApiHandler::AllowedHttpMethods> ApiHandler::allowed_methods_ = {
        {ApiMethod::GET_MAPS, {http::verb::get, http::verb::head}},
        {ApiMethod::GET_MAP, {http::verb::get, http::verb::head}},
        {ApiMethod::JOIN_GAME, {http::verb::post}},
        {ApiMethod::GET_PLAYERS, {http::verb::get, http::verb::head}},
        {ApiMethod::GET_GAME_STATE, {http::verb::get, http::verb::head}},
        {ApiMethod::SET_PLAYER_ACTION, {http::verb::post}},
        {ApiMethod::TICK, {http::verb::post}},
};

std::pair<ApiHandler::ApiMethod, std::string> ApiHandler::GetApiMethod(
    const std::string& url) {
    for (const auto& [pattern, method] : api_methods_) {
        std::smatch matches;
        if (std::regex_match(url, matches, std::regex(pattern))) {
            return {method, matches.size() > 1 ? matches[1].str() : std::string{}};
        }
    }
    return {ApiMethod::UNKNOWN, {}};
}

const ApiHandler::AllowedHttpMethods* ApiHandler::GetAllowedMethods(
    ApiMethod method) noexcept {
    if (const auto it = allowed_methods_.find(method); it != allowed_methods_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string ApiHandler::MakeAllowHeader(const AllowedHttpMethods& methods) {
    std::string result;
    for (const http::verb method : methods) {
        if (!result.empty()) {
            result += ", ";
        }
        result += http::to_string(method);
    }
    return result;
}

bool ApiHandler::IsJsonContentType(std::string_view content_type) {
    content_type = content_type.substr(0, content_type.find(';'));
    while (!content_type.empty() &&
           std::isspace(static_cast<unsigned char>(content_type.front()))) {
        content_type.remove_prefix(1);
    }
    while (!content_type.empty() &&
           std::isspace(static_cast<unsigned char>(content_type.back()))) {
        content_type.remove_suffix(1);
    }
    std::string normalized{content_type};
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "application/json";
}

app::Token ApiHandler::ParseAuthorization(std::string_view authorization) {
    constexpr std::string_view prefix = "Bearer ";
    constexpr std::size_t token_size = 32;
    if (!authorization.starts_with(prefix) ||
        authorization.size() != prefix.size() + token_size) {
        throw InvalidTokenException("Authorization header is invalid");
    }

    const std::string_view token = authorization.substr(prefix.size());
    if (!std::all_of(token.begin(), token.end(), [](unsigned char ch) {
            return std::isxdigit(ch) != 0;
        })) {
        throw InvalidTokenException("Authorization header is invalid");
    }
    return app::Token{std::string{token}};
}

json::object ApiHandler::SerializeRoad(const model::Road& road) {
    json::object result;
    if (road.IsHorizontal()) {
        result.emplace(DC::END_X, road.GetEnd().x);
    } else {
        result.emplace(DC::END_Y, road.GetEnd().y);
    }
    result.emplace(DC::START_X, road.GetStart().x);
    result.emplace(DC::START_Y, road.GetStart().y);
    return result;
}

json::object ApiHandler::SerializeBuilding(const model::Building& building) {
    const model::Rectangle& bounds = building.GetBounds();
    return {
        {DC::X, bounds.position.x},
        {DC::Y, bounds.position.y},
        {DC::WIDTH, bounds.size.width},
        {DC::HEIGHT, bounds.size.height},
    };
}

json::object ApiHandler::SerializeOffice(const model::Office& office) {
    return {
        {"id", *office.GetId()},
        {DC::X, office.GetPosition().x},
        {DC::Y, office.GetPosition().y},
        {DC::OFFSET_X, office.GetOffset().dx},
        {DC::OFFSET_Y, office.GetOffset().dy},
    };
}

json::object ApiHandler::SerializeMap(const model::Map& map, bool simple) {
    json::object result{{"id", *map.GetId()}, {DC::MAP_NAME, map.GetName()}};
    if (simple) {
        return result;
    }

    json::array roads;
    for (const model::Road& road : map.GetRoads()) {
        roads.emplace_back(SerializeRoad(road));
    }
    result.emplace(DC::ROAD_BLOCK, std::move(roads));

    json::array buildings;
    for (const model::Building& building : map.GetBuildings()) {
        buildings.emplace_back(SerializeBuilding(building));
    }
    result.emplace(DC::BUILDING_BLOCK, std::move(buildings));

    json::array offices;
    for (const model::Office& office : map.GetOffices()) {
        offices.emplace_back(SerializeOffice(office));
    }
    result.emplace(DC::OFFICE_BLOCK, std::move(offices));
    return result;
}

ApiHandler::HandlerResult ApiHandler::HandleGetMaps() const {
    json::array result;
    for (const auto& map : application_.GetMaps()) {
        result.emplace_back(SerializeMap(*map, true));
    }
    return result;
}

ApiHandler::HandlerResult ApiHandler::HandleGetMap(std::string_view map_id) const {
    const auto map = application_.FindMap(model::Map::Id{std::string{map_id}});
    if (!map) {
        throw MapNotFoundException();
    }
    return SerializeMap(*map, false);
}

ApiHandler::HandlerResult ApiHandler::HandleJoinGame(std::string_view body) {
    std::string user_name;
    std::string map_id;
    try {
        const json::value parsed_request = json::parse(json::string_view{body.data(), body.size()});
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
    if (!application_.FindMap(requested_map_id)) {
        throw MapNotFoundException();
    }

    auto [player, token] = application_.JoinGame(std::move(user_name), requested_map_id);
    return json::object{{"authToken", *token}, {"playerId", *player.GetId()}};
}

ApiHandler::HandlerResult ApiHandler::HandleGetPlayers(
    const app::Player& player) const {
    json::object result;
    const auto& players_map = application_.GetPlayersOnMap(player.GetMap().GetId());
    for (const app::Player* map_player : players_map) {
        result.emplace(
            std::to_string(*map_player->GetId()),
            json::object{{"name", map_player->GetDog().GetName()}}
        );
    }
    return result;
}

std::string_view ApiHandler::SerializeDirection(model::Direction direction) noexcept {
    switch (direction) {
        case model::Direction::NORTH:
            return "U";
        case model::Direction::SOUTH:
            return "D";
        case model::Direction::WEST:
            return "L";
        case model::Direction::EAST:
            return "R";
    }
    return "U";
}

ApiHandler::HandlerResult ApiHandler::HandleGetGameState(
    const app::Player& player) const {
    json::object players;
    const auto& players_map = application_.GetPlayersOnMap(player.GetMap().GetId());
    for (const app::Player* map_player : players_map) {
        const model::Dog& dog = map_player->GetDog();
        const model::Position position = dog.GetPosition();
        const model::Speed speed = dog.GetSpeed();
        players.emplace(
            std::to_string(*map_player->GetId()),
            json::object{
                {"pos", json::array{position.x, position.y}},
                {"speed", json::array{speed.x, speed.y}},
                {"dir", SerializeDirection(dog.GetDirection())},
            });
    }
    return json::object{{"players", std::move(players)}};
}

ApiHandler::HandlerResult ApiHandler::HandlePlayerAction(
    app::Player& player, std::string_view body) {
    std::string move;
    try {
        const json::value parsed_request = json::parse(
            json::string_view{body.data(), body.size()});
        move = json::value_to<std::string>(parsed_request.as_object().at("move"));
    } catch (const std::exception&) {
        throw InvalidArgumentException("Failed to parse action");
    }

    if (move.empty()) {
        player.Stop();
    } else if (move == "L") {
        player.Move(model::Direction::WEST);
    } else if (move == "R") {
        player.Move(model::Direction::EAST);
    } else if (move == "U") {
        player.Move(model::Direction::NORTH);
    } else if (move == "D") {
        player.Move(model::Direction::SOUTH);
    } else {
        throw InvalidArgumentException("Failed to parse action");
    }

    return json::object{};
}

ApiHandler::HandlerResult ApiHandler::HandleTick(std::string_view body) {
    std::int64_t time_delta = 0;
    try {
        const json::value parsed_request = json::parse(
            json::string_view{body.data(), body.size()});
        const json::value& delta = parsed_request.as_object().at("timeDelta");
        if (!delta.is_int64()) {
            throw std::invalid_argument("timeDelta must be an integer");
        }
        time_delta = delta.as_int64();
        if (time_delta < 0) {
            throw std::invalid_argument("timeDelta must not be negative");
        }
    } catch (const std::exception&) {
        throw InvalidArgumentException("Failed to parse tick request JSON");
    }

    application_.Tick(std::chrono::milliseconds{time_delta});
    return json::object{};
}

}  // namespace http_handler
