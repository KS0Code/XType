#pragma once

#include <functional>
#include <optional>
#include <string>

#include "config.h"

namespace fcitx { class Instance; }

// Puts Fcitx5 into the state the popup ghost style needs, and takes it back
// out again. Idempotent: call with the current config on load and every reload.
//
// - Classic UI: while popup is on, Theme/DarkTheme/Font point at
//   ghost.popup_theme / ghost.popup_font. The user's own values are saved to
//   <stateDir>/classicui-restore.conf first and restored when popup goes off.
// - Hyprland: loads the xtype-inline plugin while popup + hyprland_inline are
//   on (and Hyprland is running), unloads it otherwise.
class GhostPopupSetup {
public:
    GhostPopupSetup(fcitx::Instance *instance, std::string stateDir);

    void apply(const GhostConfig &cfg);
    void setLogSink(std::function<void(const std::string &)> log) { _log = std::move(log); }

private:
    void applyTheme(const GhostConfig &cfg, bool popup);
    void applyHyprland(bool wanted);
    void log(const std::string &msg) const { if (_log) _log(msg); }

    fcitx::Instance                         *_instance;
    std::string                              _restorePath;
    std::optional<bool>                      _hyprlandLoaded;   // unknown until first apply
    std::function<void(const std::string &)> _log;
};
