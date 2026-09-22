#pragma once

#include <boost/json.hpp>

#include <string>
#include <unordered_map>

namespace extra_data {

using MapLootTypes = std::unordered_map<std::string, boost::json::array>;

}  // namespace extra_data
