#pragma once

#include <deque>
#include <optional>
#include <string>
#include <string_view>

class ContextBuffer {
public:
    void        appendChar(char ch);
    void        appendText(std::string_view utf8);   // a typed character or composed sequence
    void        backspace();                         // removes one code point
    void        setSuggestion(std::string text);
    std::string acceptNextWord();
    std::string acceptAll();
    void        dismiss();
    void        reset();

    std::string        contextText()   const;
    const std::string* suggestion()    const;  // nullptr = no active suggestion
    bool               hasSuggestion() const;

private:
    void push(char ch);  // rolling-window insert: drops oldest code point past MAX_CHARS bytes

    std::deque<char>           _buf;
    std::optional<std::string> _suggestion;

    static constexpr size_t MAX_CHARS = 500;
};
