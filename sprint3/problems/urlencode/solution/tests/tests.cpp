#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
}

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("AZaz09-._~%\"<>^`{|}\\"sv), "AZaz09-._~%\"<>^`{|}\\"s);
    EXPECT_EQ(UrlEncode(std::string(1, '\x7f')), std::string(1, '\x7f'));
}

TEST(UrlEncodeTestSuite, ReservedCharsArePercentEncoded) {
    EXPECT_EQ(UrlEncode("!#$&'()*+,/:;=?@[]"sv),
              "%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"s);
    EXPECT_EQ(UrlEncode("Hello World!"sv), "Hello+World%21"s);
}

TEST(UrlEncodeTestSuite, SpacesBecomePlusSigns) {
    EXPECT_EQ(UrlEncode(" a  b "sv), "+a++b+"s);
    EXPECT_EQ(UrlEncode("+ +"sv), "%2B+%2B"s);
}

TEST(UrlEncodeTestSuite, ControlAndHighBytesArePercentEncoded) {
    EXPECT_EQ(UrlEncode(std::string("\0\x01\x1f\x20\x7f\x80\xff", 7)),
              std::string("%00%01%1F+\x7f%80%FF"));
}
