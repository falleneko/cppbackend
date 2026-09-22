#include "urlencode.h"

std::string UrlEncode(std::string_view str) {
    constexpr std::string_view reserved = "!#$&'()*+,/:;=?@[]";
    constexpr char hex[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(str.size());

    for (unsigned char byte : str) {
        if (byte == ' ') {
            result += '+';
        } else if (byte < 32 || byte >= 128 || reserved.find(byte) != std::string_view::npos) {
            result += '%';
            result += hex[byte >> 4];
            result += hex[byte & 0x0F];
        } else {
            result += static_cast<char>(byte);
        }
    }

    return result;
}
