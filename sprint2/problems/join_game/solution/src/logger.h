#pragma once

#include <boost/json/value.hpp>

#include <string_view>

namespace app_logging {

void InitBoostLog();
void Log(std::string_view message, boost::json::value data);

}  // namespace app_logging
