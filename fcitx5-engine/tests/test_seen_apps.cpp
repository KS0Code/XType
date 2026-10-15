#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

#include "seen_apps.h"

TEST_CASE("touch records a new app and asks for a save", "[seen_apps]") {
    SeenApps s;
    CHECK(s.touch("omawrite", 1000));
    CHECK(s.apps().at("omawrite") == 1000);
}

TEST_CASE("touch ignores empty names", "[seen_apps]") {
    SeenApps s;
    CHECK_FALSE(s.touch("", 1000));
    CHECK(s.apps().empty());
}

TEST_CASE("touch re-stamps a known app only after kRefreshSec", "[seen_apps]") {
    SeenApps s;
    s.touch("omawrite", 1000);
    CHECK_FALSE(s.touch("omawrite", 1000 + SeenApps::kRefreshSec - 1));
    CHECK(s.apps().at("omawrite") == 1000);
    CHECK(s.touch("omawrite", 1000 + SeenApps::kRefreshSec));
    CHECK(s.apps().at("omawrite") == 1000 + SeenApps::kRefreshSec);
}

TEST_CASE("touch drops the oldest app beyond kMaxApps", "[seen_apps]") {
    SeenApps s;
    for (std::size_t i = 0; i < SeenApps::kMaxApps; ++i)
        s.touch("app" + std::to_string(i), static_cast<std::time_t>(100 + i));
    s.touch("newest", 100000);
    CHECK(s.apps().size() == SeenApps::kMaxApps);
    CHECK(s.apps().count("app0") == 0);
    CHECK(s.apps().count("newest") == 1);
}

TEST_CASE("toJson / parse round-trip, most recent first", "[seen_apps]") {
    SeenApps s;
    s.touch("omawrite", 1000);
    s.touch("com.t3tools.T3Code", 2000);
    s.touch("we\"ird\\na{me}", 1500);
    const std::string json = s.toJson();
    CHECK(json.find("com.t3tools.T3Code") < json.find("omawrite"));

    SeenApps r;
    r.parse(json);
    CHECK(r.apps() == s.apps());
}

TEST_CASE("parse tolerates garbage and missing fields", "[seen_apps]") {
    SeenApps s;
    s.parse("not json at all");
    CHECK(s.apps().empty());
    s.parse(R"({"apps":[{"name":"a"},{"last_seen":5},{"name":"b","last_seen":7}]})");
    CHECK(s.apps().size() == 2);
    CHECK(s.apps().at("a") == 0);
    CHECK(s.apps().at("b") == 7);
}

TEST_CASE("save / load through a file", "[seen_apps]") {
    auto path = (std::filesystem::temp_directory_path() / "xtype_seen_apps_test.json").string();
    SeenApps s;
    s.touch("kate", 42);
    REQUIRE(s.save(path));
    SeenApps r;
    r.load(path);
    CHECK(r.apps() == s.apps());
    std::remove(path.c_str());

    SeenApps missing;
    missing.load(path);
    CHECK(missing.apps().empty());
}
