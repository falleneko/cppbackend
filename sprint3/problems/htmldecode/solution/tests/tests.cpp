#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
    CHECK(HtmlDecode("hello"sv) == "hello"s);
    CHECK(HtmlDecode("Johnson&Johnson"sv) == "Johnson&Johnson"s);
}

TEST_CASE("All supported mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt;&gt;&amp;&apos;&quot;"sv) == "<>&'\""s);
    CHECK(HtmlDecode("&lt &gt &amp &apos &quot"sv) == "< > & ' \""s);
    CHECK(HtmlDecode("&LT;&GT;&AMP;&APOS;&QUOT;"sv) == "<>&'\""s);
    CHECK(HtmlDecode("&LT &GT &AMP &APOS &QUOT"sv) == "< > & ' \""s);
}

TEST_CASE("Mnemonics at every position", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt;start"sv) == "<start"s);
    CHECK(HtmlDecode("before&gt;after"sv) == "before>after"s);
    CHECK(HtmlDecode("end&amp"sv) == "end&"s);
    CHECK(HtmlDecode("M&amp;M&APOSs"sv) == "M&M's"s);
}

TEST_CASE("Unknown and incomplete mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("& &l &ap &quo &am"sv) == "& &l &ap &quo &am"s);
    CHECK(HtmlDecode("&abracadabra; &copy; &#39;"sv) == "&abracadabra; &copy; &#39;"s);
    CHECK(HtmlDecode("&Lt; &gT; &aMp; &aPos; &QuOt;"sv)
          == "&Lt; &gT; &aMp; &aPos; &QuOt;"s);
}

TEST_CASE("Decoded ampersands are not decoded again", "[HtmlDecode]") {
    CHECK(HtmlDecode("&amp;lt; &amp;amp; &amp;apos;"sv)
          == "&lt; &amp; &apos;"s);
}

TEST_CASE("Only characters inside the string_view are decoded", "[HtmlDecode]") {
    CHECK(HtmlDecode(std::string_view("x&amp;yignored", 7)) == "x&y"s);
}
