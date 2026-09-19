#include "json_loader.h"

#include <fstream>
#include <iostream>

namespace json_loader {

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

    json::array config;
    try {
        config = json::parse(file).as_object()["maps"].as_array();
    } catch (...) {
        std::cerr << "Unable to parse JSON" << std::endl;
        throw std::current_exception();
    }

    for (const auto& val : config) {
        auto obj = val.as_object();
        model::Map::Id id{json::value_to<std::string>(obj.at("id"))};
        model::Map map{
            id,
            json::value_to<std::string>(obj.at("name"))
        };

        json::array roads = obj.at("roads").as_array();
        for (const auto& road_val : roads) {
            ParseMapRoadObj(road_val, map);
        }

        json::array buildings = obj.at("buildings").as_array();
        for (const auto& building_val : buildings) {
            json::object building = building_val.as_object();
            ParseMapBuildingObj(map, building);
        }

        json::array offices = obj.at("offices").as_array();
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
                json::value_to<int>(office.at("x")),
                json::value_to<int>(office.at("y"))
            },
            {
                json::value_to<int>(office.at("offsetX")),
                json::value_to<int>(office.at("offsetY"))
            }
        });
    }

    void ParseMapBuildingObj(model::Map &map, boost::json::object &building) {
        map.AddBuilding(model::Building{{
            {
                json::value_to<int>(building.at("x")),
                json::value_to<int>(building.at("y"))
            },
            {
                json::value_to<int>(building.at("w")),
                json::value_to<int>(building.at("h"))
            }
        }});
    }

    void ParseMapRoadObj(const boost::json::value &road_val, model::Map &map) {
        json::object road = road_val.as_object();
        if (road.contains("x1")) {
            map.AddRoad(model::Road{
                model::Road::HORIZONTAL,
                {
                    json::value_to<int>(road.at("x0")),
                    json::value_to<int>(road.at("y0"))},
                    json::value_to<int>(road.at("x1"))
                }
            );
        } else {
            map.AddRoad(model::Road{
                model::Road::VERTICAL,
                {
                    json::value_to<int>(road.at("x0")),
                    json::value_to<int>(road.at("y0"))
                },
                json::value_to<int>(road.at("y1"))
            });
        }
    }
} // namespace

} // namespace json_loader
