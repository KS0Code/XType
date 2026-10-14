#pragma once

#include <string>

// Minimal UTF-8 helpers for the typed-text buffers. The engine stores what the
// user typed as UTF-8 bytes; anything that drops or slices bytes must stay on
// code-point boundaries, or the text sent to Ollama / written to JSON becomes
// invalid UTF-8.
namespace utf8_util {

inline bool isContinuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// Removes the last code point (not just the last byte).
inline void popBack(std::string &s) {
    while (!s.empty() && isContinuation(s.back())) s.pop_back();
    if (!s.empty()) s.pop_back();
}

// The last <= maxBytes bytes of s, starting on a code-point boundary.
inline std::string tail(const std::string &s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t start = s.size() - maxBytes;
    while (start < s.size() && isContinuation(s[start])) ++start;
    return s.substr(start);
}

}  // namespace utf8_util
