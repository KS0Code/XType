#include "seen_apps.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

std::string escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) continue;  // drop other controls
                out += c;
        }
    }
    return out;
}

// Reads the JSON string starting at json[pos] == '"'. On success returns true,
// stores the unescaped text and leaves pos just past the closing quote.
bool readString(std::string_view json, size_t& pos, std::string& out) {
    if (pos >= json.size() || json[pos] != '"') return false;
    out.clear();
    for (++pos; pos < json.size(); ++pos) {
        char c = json[pos];
        if (c == '"') { ++pos; return true; }
        if (c == '\\' && pos + 1 < json.size()) {
            char e = json[++pos];
            switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                default:  out += e;          // \" \\ \/
            }
        } else {
            out += c;
        }
    }
    return false;
}

// Value of `"key": <string|number>` inside json[begin, end).
bool findString(std::string_view json, size_t begin, size_t end,
                std::string_view key, std::string& out) {
    std::string needle = "\"" + std::string(key) + "\"";
    size_t k = json.find(needle, begin);
    if (k == std::string_view::npos || k >= end) return false;
    size_t p = json.find_first_not_of(" \t\r\n:", k + needle.size());
    return p != std::string_view::npos && p < end && readString(json, p, out);
}

bool findNumber(std::string_view json, size_t begin, size_t end,
                std::string_view key, long long& out) {
    std::string needle = "\"" + std::string(key) + "\"";
    size_t k = json.find(needle, begin);
    if (k == std::string_view::npos || k >= end) return false;
    size_t p = json.find_first_not_of(" \t\r\n:", k + needle.size());
    if (p == std::string_view::npos || p >= end) return false;
    std::string digits(json.substr(p, std::min<size_t>(20, end - p)));
    char* stop = nullptr;
    out = std::strtoll(digits.c_str(), &stop, 10);
    return stop != digits.c_str();
}

}  // namespace

void SeenApps::parse(std::string_view json) {
    _apps.clear();
    // Each app is one flat {...} object. Strings are stepped over as JSON
    // strings while looking for the closing brace, so a brace inside a name
    // doesn't end the object early.
    size_t pos = json.find('[');
    while (pos != std::string_view::npos) {
        size_t open = json.find('{', pos);
        if (open == std::string_view::npos) break;
        std::string name;
        size_t end = open + 1;
        // Find the object's closing brace, stepping over strings.
        while (end < json.size() && json[end] != '}') {
            if (json[end] == '"') {
                std::string skipped;
                if (!readString(json, end, skipped)) return;
            } else {
                ++end;
            }
        }
        if (end >= json.size()) break;
        long long ts = 0;
        if (findString(json, open, end, "name", name) && !name.empty()) {
            findNumber(json, open, end, "last_seen", ts);
            _apps[name] = static_cast<std::time_t>(ts);
        }
        pos = end + 1;
    }
}

std::string SeenApps::toJson() const {
    // Most recent first, which is also the order the settings app shows.
    std::vector<std::pair<std::string, std::time_t>> list(_apps.begin(), _apps.end());
    std::stable_sort(list.begin(), list.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    std::string out = "{\"apps\":[";
    for (size_t i = 0; i < list.size(); ++i) {
        char ts[32];
        std::snprintf(ts, sizeof(ts), "%" PRId64, static_cast<int64_t>(list[i].second));
        out += i ? ",\n  " : "\n  ";
        out += "{\"name\":\"" + escape(list[i].first) + "\",\"last_seen\":" + ts + "}";
    }
    out += "\n]}\n";
    return out;
}

bool SeenApps::touch(const std::string& name, std::time_t now) {
    if (name.empty()) return false;
    auto it = _apps.find(name);
    if (it != _apps.end() && now - it->second < kRefreshSec) return false;
    _apps[name] = now;
    while (_apps.size() > kMaxApps) {
        auto oldest = std::min_element(_apps.begin(), _apps.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        _apps.erase(oldest);
    }
    return true;
}

void SeenApps::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) { _apps.clear(); return; }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    parse(content);
}

bool SeenApps::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return false;
    const std::string json = toJson();
    bool ok = std::fputs(json.c_str(), f) >= 0;
    ok = (std::fclose(f) == 0) && ok;
    return ok && std::rename(tmp.c_str(), path.c_str()) == 0;
}
