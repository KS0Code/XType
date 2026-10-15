#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <unistd.h>

#include "ghost_popup_state.h"

namespace fs = std::filesystem;

static fs::path tmpFile(const char *tag) {
    return fs::temp_directory_path() /
           ("xtype_ghost_" + std::string(tag) + "_" + std::to_string(::getpid()) + ".conf");
}

TEST_CASE("popupValues points theme, dark theme and font at the ghost config", "[ghost]") {
    GhostConfig cfg;
    cfg.popup_theme = "xtype-ghost";
    cfg.popup_font  = "Inter 12";
    const auto v = ghost_popup::popupValues(cfg);
    CHECK(v.at("Theme") == "xtype-ghost");
    CHECK(v.at("DarkTheme") == "xtype-ghost");
    CHECK(v.at("UseDarkTheme") == "False");
    CHECK(v.at("UseAccentColor") == "False");
    CHECK(v.at("Font") == "Inter 12");
}

TEST_CASE("restore file round-trips the user's values, including spaces", "[ghost]") {
    auto p = tmpFile("rt");
    ghost_popup::Values own = {{"Theme", "default-dark"}, {"Font", "Noto Sans 10"}};
    REQUIRE(ghost_popup::saveRestore(p.string(), own));
    auto back = ghost_popup::loadRestore(p.string());
    REQUIRE(back.has_value());
    CHECK(*back == own);
    fs::remove(p);
}

TEST_CASE("missing restore file means nothing was changed", "[ghost]") {
    CHECK_FALSE(ghost_popup::loadRestore(tmpFile("missing").string()).has_value());
}

TEST_CASE("empty restore file restores nothing (user had no classicui settings)", "[ghost]") {
    auto p = tmpFile("empty");
    REQUIRE(ghost_popup::saveRestore(p.string(), {}));
    auto back = ghost_popup::loadRestore(p.string());
    REQUIRE(back.has_value());
    CHECK(back->empty());
    fs::remove(p);
}
