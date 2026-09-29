#pragma once

#include <filesystem>

#include "model.h"
#include "extra_data.h"
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;
namespace DC = model::DATA_CONST;

model::Game LoadGame(const std::filesystem::path& json_path,
                     extra_data::MapLootTypes* loot_types = nullptr);

}  // namespace json_loader
