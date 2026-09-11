#include "json_loader.h"

#include <fstream>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
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
    json::array config = json::parse(file).as_object()["maps"].as_array();

    for (const auto& val : config) {
        auto obj = val.as_object();
        model::Map::Id id{json::value_to<std::string>(obj.at("id"))};
        model::Map map{
            id,
            json::value_to<std::string>(obj.at("name"))
        };

        json::array roads = obj.at("roads").as_array();
        for (const auto& road_val : roads) {
            json::object road = road_val.as_object();
            if (road.contains("x1")) {
                map.AddRoad(model::Road{
                    model::Road::HORIZONTAL,
                    {
                        json::value_to<int>(road.at("x0")),
                        json::value_to<int>(road.at("y0"))
                    },
                    json::value_to<int>(road.at("x1"))
                });
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

        json::array buildings = obj.at("buildings").as_array();
        for (const auto& building_val : buildings) {
            json::object building = building_val.as_object();
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

        json::array offices = obj.at("offices").as_array();
        for (const auto& office_val : offices) {
            json::object office = office_val.as_object();

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
        
        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
