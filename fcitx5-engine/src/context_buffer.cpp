#include "context_buffer.h"

#include "utf8_util.h"

// ── Private ───────────────────────────────────────────────────────────────────

void ContextBuffer::push(char ch) {
    _buf.push_back(ch);
    if (_buf.size() > MAX_CHARS) {
        _buf.pop_front();
        // Never leave the tail bytes of a multi-byte character at the front.
        while (!_buf.empty() && utf8_util::isContinuation(_buf.front()))
            _buf.pop_front();
    }
}

// ── Queries ───────────────────────────────────────────────────────────────────

std::string ContextBuffer::contextText() const {
    return {_buf.begin(), _buf.end()};
}

const std::string* ContextBuffer::suggestion() const {
    return _suggestion ? &*_suggestion : nullptr;
}

bool ContextBuffer::hasSuggestion() const {
    return _suggestion.has_value();
}

// ── Buffer mutations ──────────────────────────────────────────────────────────

void ContextBuffer::appendChar(char ch) {
    dismiss();
    push(ch);
}

void ContextBuffer::appendText(std::string_view utf8) {
    dismiss();
    for (char ch : utf8)
        push(ch);
}

void ContextBuffer::backspace() {
    if (_suggestion) {
        dismiss();
    } else {
        while (!_buf.empty() && utf8_util::isContinuation(_buf.back()))
            _buf.pop_back();
        if (!_buf.empty())
            _buf.pop_back();
    }
}

// ── Suggestion lifecycle ──────────────────────────────────────────────────────

void ContextBuffer::setSuggestion(std::string text) {
    if (text.empty())
        _suggestion = std::nullopt;
    else
        _suggestion = std::move(text);
}

void ContextBuffer::dismiss() {
    _suggestion = std::nullopt;
}

void ContextBuffer::reset() {
    _buf.clear();
    _suggestion = std::nullopt;
}

std::string ContextBuffer::acceptNextWord() {
    if (!_suggestion) return {};

    auto pos = _suggestion->find(' ');

    std::string word;
    if (pos == std::string::npos) {
        word = std::move(*_suggestion);
        _suggestion = std::nullopt;
    } else {
        word             = _suggestion->substr(0, pos + 1);
        auto remaining   = _suggestion->substr(pos + 1);
        _suggestion      = remaining.empty()
                           ? std::nullopt
                           : std::optional<std::string>(std::move(remaining));
    }

    for (char ch : word)
        push(ch);

    return word;
}

std::string ContextBuffer::acceptAll() {
    if (!_suggestion) return {};

    std::string text = std::move(*_suggestion);
    _suggestion = std::nullopt;

    for (char ch : text)
        push(ch);

    return text;
}
