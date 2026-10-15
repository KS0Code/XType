#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "config_loader.h"

TEST_CASE("empty file returns all defaults") {
    auto cfg = config_loader::load("tests/fixtures/config_defaults.toml");
    CHECK(cfg.inference.model == "qwen2.5:1.5b");
    CHECK(cfg.inference.debounce_ms == 220);
    CHECK(cfg.inference.temperature == Catch::Approx(0.3f).margin(0.001));
    CHECK(cfg.behaviour.engine_enabled == true);
    CHECK(cfg.behaviour.accept_full_key == AcceptKey::Tab);
    CHECK(cfg.learning.enabled == false);
    CHECK(cfg.user_prompt.description.empty());
    CHECK(cfg.apps.empty());
}

TEST_CASE("full config round-trips every field") {
    auto cfg = config_loader::load("tests/fixtures/config_full.toml");
    CHECK(cfg.inference.model == "llama3:8b");
    CHECK(cfg.inference.debounce_ms == 400);
    CHECK(cfg.inference.temperature == Catch::Approx(0.7f).margin(0.001));
    CHECK(cfg.inference.top_p == Catch::Approx(0.8f).margin(0.001));
    CHECK(cfg.inference.threads.has_value());
    CHECK(*cfg.inference.threads == 4);
    CHECK(cfg.inference.stop_tokens == std::vector<std::string>{".", "\n"});
    CHECK(cfg.behaviour.engine_enabled == false);
    CHECK(cfg.behaviour.trigger_mode == TriggerMode::Manual);
    CHECK(cfg.behaviour.accept_full_key == AcceptKey::Enter);
    CHECK(cfg.behaviour.partial_accept == false);
    CHECK(cfg.behaviour.blocklist_apps == std::vector<std::string>{"kate", "subl"});
    CHECK(cfg.behaviour.blocked_phrases == std::vector<std::string>{"pursuant to", "hereinafter"});
    CHECK(cfg.learning.enabled == true);
    CHECK(cfg.learning.max_corpus_mb == 25);
    CHECK(cfg.learning.voice_strength == 80);
    CHECK(cfg.learning.forget_after_days == 14);
    CHECK(cfg.user_prompt.description == "A senior software engineer who writes tersely.");
    CHECK(cfg.user_prompt.tone == "technical");
    CHECK(cfg.user_prompt.avoid_phrases == std::vector<std::string>{"synergy", "leverage"});
    CHECK(cfg.apps.count("kate") == 1);
    CHECK(*cfg.apps.at("kate").enabled == false);
    CHECK(*cfg.apps.at("kate").mode == "code-aware");
    CHECK(*cfg.apps.at("kate").num_predict == 20);
    CHECK(*cfg.apps.at("kate").debounce_ms == 100);
    CHECK(cfg.apps.count("org.kde.konsole") == 1);
}

TEST_CASE("legacy tab_accepts_word migrates to partial_accept") {
    auto cfg = config_loader::load("tests/fixtures/config_legacy.toml");
    CHECK(cfg.behaviour.partial_accept == false);
}

TEST_CASE("missing file returns defaults without error") {
    auto cfg = config_loader::load("/tmp/xtype_nonexistent_test.toml");
    CHECK(cfg.inference.model == "qwen2.5:1.5b");
}

TEST_CASE("malformed TOML returns defaults without crash") {
    const char* path = "/tmp/xtype_bad_test.toml";
    FILE* f = std::fopen(path, "w");
    std::fputs("[inference\nbad = !!!\n", f);
    std::fclose(f);
    auto cfg = config_loader::load(path);
    CHECK(cfg.inference.model == "qwen2.5:1.5b");
    std::remove(path);
}

TEST_CASE("ghost: defaults are inline style with the xtype-ghost popup theme") {
    auto cfg = config_loader::load("tests/fixtures/config_defaults.toml");
    CHECK(cfg.ghost.style == GhostStyle::Inline);
    CHECK(cfg.ghost.popup_font == "Sans 11");
    CHECK(cfg.ghost.popup_theme == "xtype-ghost");
    CHECK(cfg.ghost.hyprland_inline == false);
}

TEST_CASE("ghost: [ghost] section is read") {
    auto cfg = config_loader::load("tests/fixtures/config_ghost.toml");
    CHECK(cfg.ghost.style == GhostStyle::Popup);
    CHECK(cfg.ghost.popup_font == "Inter 12");
    CHECK(cfg.ghost.popup_theme == "my-theme");
    CHECK(cfg.ghost.hyprland_inline == true);
}

TEST_CASE("ghost: per-app ghost_style overrides [ghost] style") {
    auto full = config_loader::load("tests/fixtures/config_full.toml");   // [ghost] inline
    CHECK(*full.apps.at("kate").ghost_style == GhostStyle::Popup);
    CHECK_FALSE(full.apps.at("org.kde.konsole").ghost_style.has_value());
    CHECK(config_loader::ghostStyleFor(full, &full.apps.at("kate")) == GhostStyle::Popup);
    CHECK(config_loader::ghostStyleFor(full, &full.apps.at("org.kde.konsole")) == GhostStyle::Inline);
    CHECK(config_loader::ghostStyleFor(full, nullptr) == GhostStyle::Inline);

    auto ghost = config_loader::load("tests/fixtures/config_ghost.toml");  // [ghost] popup
    CHECK(config_loader::ghostStyleFor(ghost, &ghost.apps.at("omawrite")) == GhostStyle::Inline);
    CHECK(config_loader::ghostStyleFor(ghost, nullptr) == GhostStyle::Popup);
}

TEST_CASE("ghost: popup setup is on when any app uses popup") {
    auto defaults = config_loader::load("tests/fixtures/config_defaults.toml");
    CHECK(config_loader::ghostSetupConfig(defaults).style == GhostStyle::Inline);

    auto full = config_loader::load("tests/fixtures/config_full.toml");   // only kate is popup
    CHECK(config_loader::ghostSetupConfig(full).style == GhostStyle::Popup);
    CHECK(full.ghost.style == GhostStyle::Inline);                         // [ghost] itself untouched
}
