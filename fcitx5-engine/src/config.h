#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

enum class TriggerMode { Pause, Manual };
// How the suggestion is shown. Inline: as preedit in the app's own text field
// (the app styles it — usually underlined). Popup: drawn by Fcitx5's UI in a
// popup at the caret, so a Fcitx5 theme controls its look (gray text).
enum class GhostStyle { Inline, Popup };

enum class AcceptKey   { Tab, Enter, Right };

struct InferenceConfig {
    std::string              model             = "qwen2.5:1.5b";
    std::string              ollama_host       = "http://localhost:11434";
    int                      debounce_ms       = 220;
    int                      min_context_chars = 10;
    int                      context_window    = 150;
    int                      num_predict       = 30;
    float                    temperature       = 0.3f;
    float                    top_p             = 0.9f;
    std::vector<std::string> stop_tokens       = {".", "!", "?", "\n"};
    std::optional<int>       threads;            // unset = let Ollama decide
};

struct BehaviourConfig {
    bool                     engine_enabled       = true;
    TriggerMode              trigger_mode         = TriggerMode::Pause;
    AcceptKey                accept_full_key      = AcceptKey::Tab;
    bool                     partial_accept       = true;   // renamed from tab_accepts_word
    bool                     esc_dismisses        = true;
    bool                     passthrough_terminals= true;
    std::vector<std::string> blocklist_apps       = {
        "konsole", "alacritty",
        "keepassxc", "1password", "bitwarden", "gnome-keyring", "seahorse"
    };
    std::vector<std::string> blocked_phrases      = {};
};

struct LearningConfig {
    bool        enabled                    = false;
    std::string corpus_path                = "~/.local/share/xtype/corpus.txt";
    int         flush_interval_sec         = 60;
    int         max_corpus_mb              = 50;
    int         min_sentence_chars         = 12;
    bool        include_examples_in_prompt = true;
    int         voice_strength             = 50;   // 0-100; engine does not read yet (comingSoon)
    int         forget_after_days          = 0;    // 0 = disabled; engine does not read yet (comingSoon)
};

struct UserPromptConfig {
    std::string              description;
    std::vector<std::string> avoid_phrases;
    std::string              tone;
};

struct AppOverride {
    std::optional<bool>        enabled;
    std::optional<std::string> model;
    std::optional<int>         debounce_ms;
    std::optional<int>         num_predict;
    std::optional<std::string> mode;            // Default/Code-aware/Email tone/Casual/Off
    std::optional<std::string> prompt_addendum;
    std::optional<GhostStyle>  ghost_style;     // overrides [ghost] style for this app
};

struct GhostConfig {
    GhostStyle  style           = GhostStyle::Inline;
    // Popup style only. While it is on, XType switches Fcitx5's classic UI to
    // popup_theme / popup_font and restores the user's own settings after.
    std::string popup_font      = "Sans 11";
    std::string popup_theme     = "xtype-ghost";
    // Popup style on Hyprland only: load the xtype-inline Hyprland plugin,
    // which moves the popup from below the caret onto the caret's line. Only
    // native Wayland text-input apps are affected (not X11, nor Qt apps using
    // Fcitx5's Qt module), and Chromium-based apps report a stale caret.
    bool        hyprland_inline = false;
};

struct XTypeConfig {
    InferenceConfig                         inference;
    BehaviourConfig                         behaviour;
    GhostConfig                             ghost;
    LearningConfig                          learning;
    UserPromptConfig                        user_prompt;
    std::map<std::string, AppOverride>      apps;
};
