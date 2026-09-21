#include "json_loader.h"

#include <fstream>
#include <iostream>

namespace json_loader {

namespace {

void ParseMapOfficeObj(model::Map& map, json::object& office);
void ParseMapBuildingObj(model::Map& map, json::object& building);
void ParseMapRoadObj(const json::value& road_val, model::Map& map);

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;

    std::ifstream file_stream(json_path);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + json_path.string());
    }
    std::string file;
    while (!file_stream.eof()) {
        std::string line;
        std::getline(file_stream, line);
        file += line;
    }

    json::object config;
    try {
        config = json::parse(file).as_object();
    } catch (...) {
        std::cerr << "Unable to parse JSON" << std::endl;
        throw std::current_exception();
    }

    const double default_dog_speed = config.contains(DC::DEF_DOG_SPEED)
        ? json::value_to<double>(config.at(DC::DEF_DOG_SPEED))
        : 1.0;

    for (const auto& val : config.at(DC::MAP_BLOCK).as_array()) {
        auto obj = val.as_object();
        model::Map::Id id{json::value_to<std::string>(obj.at("id"))};
        model::Map map{
            id,
            json::value_to<std::string>(obj.at(DC::MAP_NAME)),
            obj.contains(DC::DOG_SPEED)
                ? json::value_to<double>(obj.at(DC::DOG_SPEED))
                : default_dog_speed
        };

        json::array roads = obj.at(DC::ROAD_BLOCK).as_array();
        for (const auto& road_val : roads) {
            ParseMapRoadObj(road_val, map);
        }

        json::array buildings = obj.at(DC::BUILDING_BLOCK).as_array();
        for (const auto& building_val : buildings) {
            json::object building = building_val.as_object();
            ParseMapBuildingObj(map, building);
        }

        json::array offices = obj.at(DC::OFFICE_BLOCK).as_array();
        for (const auto& office_val : offices) {
            json::object office = office_val.as_object();
            ParseMapOfficeObj(map, office);
        }
        
        game.AddMap(std::move(map));
    }

    return game;
}

namespace
{
    void ParseMapOfficeObj(model::Map &map, boost::json::object &office) {
        map.AddOffice(model::Office{
            model::Office::Id(json::value_to<std::string>(office.at("id"))),
            {
                json::value_to<int>(office.at(DC::X)),
                json::value_to<int>(office.at(DC::Y))
            },
            {
                json::value_to<int>(office.at(DC::OFFSET_X)),
                json::value_to<int>(office.at(DC::OFFSET_Y))
            }
        });
    }

    void ParseMapBuildingObj(model::Map &map, boost::json::object &building) {
        map.AddBuilding(model::Building{{
            {
                json::value_to<int>(building.at(DC::X)),
                json::value_to<int>(building.at(DC::Y))
            },
            {
                json::value_to<int>(building.at(DC::WIDTH)),
                json::value_to<int>(building.at(DC::HEIGHT))
            }
        }});
    }

    void ParseMapRoadObj(const boost::json::value &road_val, model::Map &map) {
        json::object road = road_val.as_object();
        if (road.contains(DC::END_X)) {
            map.AddRoad(model::Road{
                model::Road::HORIZONTAL,
                {
                    json::value_to<int>(road.at(DC::START_X)),
                    json::value_to<int>(road.at(DC::START_Y))},
                    json::value_to<int>(road.at(DC::END_X))
                }
            );
        } else {
            map.AddRoad(model::Road{
                model::Road::VERTICAL,
                {
                    json::value_to<int>(road.at(DC::START_X)),
                    json::value_to<int>(road.at(DC::START_Y))
                },
                json::value_to<int>(road.at(DC::END_Y))
            });
        }
    }
} // namespace

} // namespace json_loader
