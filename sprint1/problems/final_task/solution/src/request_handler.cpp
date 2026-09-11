#include "request_handler.h"
#include <regex>

namespace http_handler {
    using namespace std::literals;

    std::string RequestHandler::API_URL = "/api/";

    const std::unordered_map<std::string, RequestHandler::AllowedHttpMethods> RequestHandler::api_methods_ = {
        {R"(v1\/maps$)", {RequestHandler::ApiMethod::GET_MAPS, http::verb::get}},
        {R"(v1\/maps\/(.*[^\/])$)", {RequestHandler::ApiMethod::GET_MAP, http::verb::get}},
    };

    std::tuple<RequestHandler::AllowedHttpMethods, std::string> RequestHandler::GetApiMethod(std::string url) {
        url = url.substr(API_URL.length(), url.length());
        for (const auto& [pattern, method] : api_methods_) {
            std::smatch matches;
            if (std::regex_match(url, matches, std::regex(pattern))) {
                return {method, matches.size() > 1 ? matches[1].str() : std::string{}};
            }
        }
        return {{ApiMethod::UNKNOWN, http::verb::unknown}, ""};
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
            json::object road_obj;
            if (road.IsHorizontal()) {
                road_obj.emplace("x1", road.GetEnd().x);
            } else if (road.IsVertical()) {
                road_obj.emplace("y1", road.GetEnd().y);
            }
            road_obj.emplace("x0", road.GetStart().x);
            road_obj.emplace("y0", road.GetStart().y);
            res["roads"].as_array().emplace_back(std::move(road_obj));
        }
        auto buildings = map.GetBuildings();
        res["buildings"] = json::array();
        for (const auto& building : buildings) {
            json::object building_obj;
            building_obj.emplace("x", building.GetBounds().position.x);
            building_obj.emplace("y", building.GetBounds().position.y);
            building_obj.emplace("w", building.GetBounds().size.width);
            building_obj.emplace("h", building.GetBounds().size.height);
            res["buildings"].as_array().emplace_back(std::move(building_obj));
        }
        auto offices = map.GetOffices();
        res["offices"] = json::array();
        for (const auto& office : offices) {
            json::object office_obj;
            office_obj.emplace("id", *office.GetId());
            office_obj.emplace("x", office.GetPosition().x);
            office_obj.emplace("y", office.GetPosition().y);
            office_obj.emplace("offsetX", office.GetOffset().dx);
            office_obj.emplace("offsetY", office.GetOffset().dy);
            res["offices"].as_array().emplace_back(std::move(office_obj));
        }
        return res;
    }

    RequestHandler::HandlerResult RequestHandler::HandleGetMaps(ApiMethod method) {
        json::array res;
        auto maps = game_.GetMaps();
        for (auto& map : maps) {
            res.emplace_back(SerializeMap(map, true));
        }        
        return res;
    }

    RequestHandler::HandlerResult RequestHandler::HandleGetMap(ApiMethod method, std::string_view map_id) {
        const model::Map* map = game_.FindMap(
            model::Map::Id{std::string{map_id}}
        );
        if (!map) {
            throw MapNotFoundException();
        }
        return SerializeMap(*map, false);
    }

}  // namespace http_handler
