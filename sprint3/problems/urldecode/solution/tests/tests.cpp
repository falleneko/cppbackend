#define BOOST_TEST_MODULE urldecode_tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

#include <stdexcept>

BOOST_AUTO_TEST_CASE(EmptyAndUnencodedStrings) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);
    BOOST_TEST(UrlDecode("AZaz09-_.~"sv) == "AZaz09-_.~"s);
    BOOST_TEST(UrlDecode("Hello World !#$&'()*,/:;=?@[]"sv)
               == "Hello World !#$&'()*,/:;=?@[]"s);
}

BOOST_AUTO_TEST_CASE(PercentEncoding) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode("Hello%20World%20%21"sv) == "Hello World !"s);
    BOOST_TEST(UrlDecode("%41%4a%4A%7e%7E%2f%2F"sv) == "AJJ~~//"s);
    BOOST_TEST(UrlDecode("A%00B"sv) == std::string("A\0B", 3));
    BOOST_TEST(UrlDecode("%FF"sv) == std::string(1, static_cast<char>(0xff)));
}

BOOST_AUTO_TEST_CASE(PlusSign) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("a++b%2Bc%2b+"sv) == "a  b+c+ "s);
}

BOOST_AUTO_TEST_CASE(InvalidPercentEncoding) {
    using namespace std::literals;

    BOOST_CHECK_THROW(UrlDecode("%GG"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2G"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%G2"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%-1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%+1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("ok%20bad%zz"sv), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(IncompletePercentEncoding) {
    using namespace std::literals;

    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%A"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("text%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("text%2"sv), std::invalid_argument);
}
