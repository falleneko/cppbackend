#include "urldecode.h"

#include <charconv>
#include <cstddef>
#include <stdexcept>

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            result.push_back(' ');
        } else if (str[i] == '%') {
            if (str.size() - i < 3) {
                throw std::invalid_argument("Incomplete percent encoding");
            }

            unsigned int value = 0;
            const char* first = str.data() + i + 1;
            const char* last = first + 2;
            const auto [end, error] = std::from_chars(first, last, value, 16);
            if (error != std::errc{} || end != last) {
                throw std::invalid_argument("Invalid percent encoding");
            }

            result.push_back(static_cast<char>(value));
            i += 2;
        } else {
            result.push_back(str[i]);
        }
    }

    return result;
}
