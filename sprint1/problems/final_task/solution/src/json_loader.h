#pragma once

#include <filesystem>

#include "model.h"
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path);

namespace {
    void ParseMapOfficeObj(model::Map &map, boost::json::object &office);
    void ParseMapBuildingObj(model::Map &map, boost::json::object &building);
    void ParseMapRoadObj(const boost::json::value &road_val, model::Map &map);
} // namespace

}  // namespace json_loader
