#pragma once

#include <filesystem>

#include "model.h"
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
