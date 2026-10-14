#include <catch2/catch_test_macros.hpp>

#include <string>

#include "utf8_util.h"

TEST_CASE("popBack removes one code point", "[utf8_util]") {
    std::string s = "aç€";
    utf8_util::popBack(s);
    REQUIRE(s == "aç");
    utf8_util::popBack(s);
    REQUIRE(s == "a");
    utf8_util::popBack(s);
    REQUIRE(s.empty());
    utf8_util::popBack(s);                    // empty: no-op
    REQUIRE(s.empty());
}

TEST_CASE("tail returns whole string when short enough", "[utf8_util]") {
    REQUIRE(utf8_util::tail("ção", 10) == "ção");
}

TEST_CASE("tail skips a split leading character", "[utf8_util]") {
    // "x€x": x(1) €(3) x(1) = 5 bytes; the last 3 bytes start inside the €.
    REQUIRE(utf8_util::tail("x€x", 3) == "x");
    REQUIRE(utf8_util::tail("x€x", 4) == "€x");
}

TEST_CASE("tail of ASCII is a plain suffix", "[utf8_util]") {
    REQUIRE(utf8_util::tail("hello world", 5) == "world");
}
