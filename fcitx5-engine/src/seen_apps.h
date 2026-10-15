#pragma once

// Apps the user has typed in, with when each was last used, so the settings
// app can list them on its Per-app page instead of a fixed list.
//
// Persisted as ~/.local/share/xtype/active_apps.json:
//   {"apps":[{"name":"com.t3tools.T3Code","last_seen":1792023274}, ...]}
// `name` is the program name Fcitx5 reports (InputContext::program()), the
// same key [apps."<name>"] uses in config.toml.
//
// Main-thread only. Approach after K-Sugar's active_apps.json (094cb35).

#include <cstddef>
#include <ctime>
#include <map>
#include <string>
#include <string_view>

class SeenApps {
public:
    // Re-stamp a known app at most this often, to keep file writes rare.
    static constexpr std::time_t kRefreshSec = 600;
    // Oldest apps are dropped beyond this many.
    static constexpr std::size_t kMaxApps    = 200;

    // Replaces the contents with what `json` holds; unparseable parts are skipped.
    void parse(std::string_view json);
    std::string toJson() const;

    // Records `name` as used at `now`. True when the stored list changed
    // enough to be worth saving: a new app, or a stamp older than kRefreshSec.
    bool touch(const std::string& name, std::time_t now);

    void load(const std::string& path);         // missing/unreadable file → empty
    bool save(const std::string& path) const;   // atomic (tmp + rename)

    const std::map<std::string, std::time_t>& apps() const { return _apps; }

private:
    std::map<std::string, std::time_t> _apps;
};
