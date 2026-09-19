#include "logger.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json/serialize.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <iostream>
#include <utility>

namespace app_logging {
namespace json = boost::json;
namespace logging = boost::log;
namespace expr = logging::expressions;
namespace keywords = logging::keywords;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

void JsonFormatter(const logging::record_view& record,
                   logging::formatting_ostream& stream) {
    json::object log_entry{
        {"timestamp", boost::posix_time::to_iso_extended_string(*record[timestamp])},
        {"data", *record[additional_data]},
        {"message", record[expr::smessage].get()},
    };
    stream << json::serialize(log_entry);
}

void InitBoostLog() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true
    );
}

void Log(std::string_view message, json::value data) {
    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, std::move(data))
        << message;
}

}  // namespace app_logging
