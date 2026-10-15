#include "ghost_popup.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#include <fcitx-config/configuration.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/misc.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/instance.h>

#include "ghost_popup_state.h"

#ifndef XTYPE_HYPRLAND_PLUGIN_PATH
#define XTYPE_HYPRLAND_PLUGIN_PATH ""
#endif

GhostPopupSetup::GhostPopupSetup(fcitx::Instance *instance, std::string stateDir)
    : _instance(instance),
      _restorePath((std::filesystem::path(stateDir) / "classicui-restore.conf").string()) {}

void GhostPopupSetup::apply(const GhostConfig &cfg) {
    const bool popup = cfg.style == GhostStyle::Popup;
    applyTheme(cfg, popup);
    applyHyprland(popup && cfg.hyprland_inline);
}

void GhostPopupSetup::applyTheme(const GhostConfig &cfg, bool popup) {
    auto *ui = _instance->addonManager().addon("classicui", true);
    if (!ui || !ui->getConfig()) {
        if (popup) log("classicui not available — popup style needs Fcitx5's classic UI");
        return;
    }
    fcitx::RawConfig raw;
    ui->getConfig()->save(raw);

    const auto saved = ghost_popup::loadRestore(_restorePath);
    if (popup) {
        // Save the user's values once, before the first change. A restore
        // file that already exists means the values in classicui are ours.
        if (!saved) {
            ghost_popup::Values own;
            for (const auto &[key, _] : ghost_popup::popupValues(cfg))
                if (const auto *v = raw.valueByPath(key)) own[key] = *v;
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(_restorePath).parent_path(), ec);
            if (!ghost_popup::saveRestore(_restorePath, own)) {
                log("could not save classicui settings; not changing the theme");
                return;
            }
        }
        bool changed = false;
        for (const auto &[key, value] : ghost_popup::popupValues(cfg)) {
            const auto *cur = raw.valueByPath(key);
            if (!cur || *cur != value) { raw.setValueByPath(key, value); changed = true; }
        }
        if (changed) {
            ui->setConfig(raw);
            log("classicui switched to theme '" + cfg.popup_theme + "' font '" + cfg.popup_font + "'");
        }
    } else if (saved) {
        for (const auto &[key, value] : *saved) raw.setValueByPath(key, value);
        ui->setConfig(raw);
        std::error_code ec;
        std::filesystem::remove(_restorePath, ec);
        log("classicui settings restored");
    }
}

void GhostPopupSetup::applyHyprland(bool wanted) {
    const std::string plugin = XTYPE_HYPRLAND_PLUGIN_PATH;
    if (!std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        if (wanted) log("hyprland_inline is on but Hyprland isn't running; ignoring");
        return;
    }
    if (plugin.empty() || !std::filesystem::exists(plugin)) {
        if (wanted) log("hyprland_inline is on but the xtype-inline plugin isn't installed");
        return;
    }
    if (_hyprlandLoaded == wanted) return;
    // Fire-and-forget: hyprctl talks to the compositor; nothing here waits on it.
    // Loading an already-loaded plugin / unloading an absent one only errors.
    fcitx::startProcess({"hyprctl", "plugin", wanted ? "load" : "unload", plugin});
    _hyprlandLoaded = wanted;
    log(std::string("hyprctl plugin ") + (wanted ? "load " : "unload ") + plugin);
}
