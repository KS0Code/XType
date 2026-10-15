#pragma once

// Fcitx5-free helpers for the popup ghost style's classic-UI settings, so the
// save/restore logic can be unit-tested without a running Fcitx5.

#include <fstream>
#include <map>
#include <optional>
#include <string>

#include "config.h"

namespace ghost_popup {

using Values = std::map<std::string, std::string>;

// The classicui options XType sets while the popup style is on.
inline Values popupValues(const GhostConfig &cfg) {
    return {
        {"Theme",          cfg.popup_theme},
        {"DarkTheme",      cfg.popup_theme},
        {"UseDarkTheme",   "False"},
        {"UseAccentColor", "False"},   // accent colours would tint the transparent theme
        {"Font",           cfg.popup_font},
    };
}

// The user's own values, saved before XType first changes them. One
// "Key<TAB>Value" per line; absent keys were unset and are left alone on restore.
inline bool saveRestore(const std::string &path, const Values &values) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto &[k, v] : values) out << k << '\t' << v << '\n';
    return static_cast<bool>(out);
}

inline std::optional<Values> loadRestore(const std::string &path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    Values values;
    std::string line;
    while (std::getline(in, line)) {
        auto tab = line.find('\t');
        if (tab != std::string::npos) values[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return values;
}

}  // namespace ghost_popup
