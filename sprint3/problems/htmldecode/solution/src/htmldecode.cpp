#include "htmldecode.h"

#include <array>
#include <utility>

std::string HtmlDecode(std::string_view str) {
    constexpr std::array<std::pair<std::string_view, char>, 10> entities{{
        {"&lt", '<'}, {"&LT", '<'},
        {"&gt", '>'}, {"&GT", '>'},
        {"&amp", '&'}, {"&AMP", '&'},
        {"&apos", '\''}, {"&APOS", '\''},
        {"&quot", '"'}, {"&QUOT", '"'},
    }};

    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size();) {
        bool decoded = false;
        if (str[i] == '&') {
            for (const auto& [entity, character] : entities) {
                if (str.substr(i).starts_with(entity)) {
                    result.push_back(character);
                    i += entity.size();
                    if (i < str.size() && str[i] == ';') {
                        ++i;
                    }
                    decoded = true;
                    break;
                }
            }
        }

        if (!decoded) {
            result.push_back(str[i]);
            ++i;
        }
    }

    return result;
}
