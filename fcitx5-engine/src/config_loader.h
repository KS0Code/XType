#pragma once
#include "config.h"
#include <string>

namespace config_loader {

// Reads ~/.config/xtype/config.toml (or `path` if non-empty override).
// Returns XTypeConfig{} defaults for any missing key or on parse error.
// Thread-safe after one-time static init of path_utils.
XTypeConfig load(const std::string& path = "");

// Returns the canonical config path: ~/.config/xtype/config.toml
// Expands ~ using $HOME; returns empty string on failure.
std::string default_path();

// The suggestion style for an app: its [apps."<name>"] ghost_style if set,
// else [ghost] style. `ov` may be null (no override for the app).
GhostStyle ghostStyleFor(const XTypeConfig& cfg, const AppOverride* ov);

// What the popup setup (classicui theme, Hyprland plugin) should apply:
// [ghost] with style = Popup when [ghost] or any app's override is popup.
// The setup stays on across app switches; each app's own style then picks
// popup or inline when a suggestion is shown.
GhostConfig ghostSetupConfig(const XTypeConfig& cfg);

}  // namespace config_loader
