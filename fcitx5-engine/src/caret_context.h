#pragma once

// Pure helpers for the text a suggestion continues. Header-only and
// Fcitx5-free so tests can use them directly.

#include <optional>
#include <string>

namespace caret_context {

inline bool isContinuation(char c) {   // UTF-8 continuation byte (10xxxxxx)
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// The app's own text before the caret, from Fcitx5 surrounding text: `text`
// with the caret `cursorChars` code points in. nullopt when it can't be
// trusted — the caret is past the end, or the text before it doesn't end with
// `lastTyped` (the app hasn't reported the keystroke we just saw yet).
inline std::optional<std::string> beforeCaret(const std::string &text,
                                              unsigned           cursorChars,
                                              const std::string &lastTyped) {
    size_t bytes = 0;
    for (unsigned n = 0; n < cursorChars; ++n) {
        if (bytes >= text.size()) return std::nullopt;
        ++bytes;
        while (bytes < text.size() && isContinuation(text[bytes])) ++bytes;
    }
    std::string before = text.substr(0, bytes);
    if (before.size() < lastTyped.size() ||
        before.compare(before.size() - lastTyped.size(), lastTyped.size(), lastTyped) != 0)
        return std::nullopt;
    return before;
}

// The last code point of s, or "" if s is empty.
inline std::string lastChar(const std::string &s) {
    if (s.empty()) return {};
    size_t start = s.size() - 1;
    while (start > 0 && isContinuation(s[start])) --start;
    return s.substr(start);
}

inline bool isBlank(char c) { return c == ' ' || c == '\t'; }

// The prompt to send for `context`: without trailing spaces/tabs. Tokenizers
// attach a word's leading space to the word, so a prompt ending on a bare
// space pushes the model toward digits and word fragments ("ritten", "0800").
// The model then emits " word"; trimLeadingBlanksAfter() drops that space
// against the real context.
inline std::string promptFor(const std::string &context) {
    size_t end = context.size();
    while (end > 0 && isBlank(context[end - 1])) --end;
    return context.substr(0, end);
}

// The model's continuation from its raw streamed output. Some chat templates
// (e.g. Qwen3 in Ollama) return the assistant prefill in front of the
// continuation; that echo is cut off. While the stream is still inside the
// echo, nothing is shown yet.
inline std::string withoutPromptEcho(const std::string &raw, const std::string &prompt) {
    if (prompt.empty()) return raw;
    if (raw.size() >= prompt.size())
        return raw.compare(0, prompt.size(), prompt) == 0 ? raw.substr(prompt.size()) : raw;
    return prompt.compare(0, raw.size(), raw) == 0 ? std::string{} : raw;
}

// A suggestion's leading spaces/tabs when the context already ends in
// whitespace — accepting them would type a double space. Mid-word and
// after-word contexts keep the model's spacing as is.
inline void trimLeadingBlanksAfter(std::string &suggestion, const std::string &context) {
    if (context.empty()) return;
    const char last = context.back();
    if (!isBlank(last) && last != '\n') return;
    size_t n = 0;
    while (n < suggestion.size() && isBlank(suggestion[n])) ++n;
    suggestion.erase(0, n);
}

}  // namespace caret_context
