#include <catch2/catch_test_macros.hpp>

#include <string>

#include "caret_context.h"

using caret_context::beforeCaret;
using caret_context::lastChar;
using caret_context::promptFor;
using caret_context::withoutPromptEcho;
using caret_context::trimLeadingBlanksAfter;

TEST_CASE("beforeCaret returns the text up to the caret", "[caret_context]") {
    auto r = beforeCaret("Hello world, again", 11, "d");
    REQUIRE(r.has_value());
    REQUIRE(*r == "Hello world");
}

TEST_CASE("beforeCaret counts the caret in code points, not bytes", "[caret_context]") {
    // "ação ok": a ç ã o (4 chars, 6 bytes) then " ok".
    auto r = beforeCaret("ação ok", 4, "o");
    REQUIRE(r.has_value());
    REQUIRE(*r == "ação");
}

TEST_CASE("beforeCaret accepts a caret at the very end", "[caret_context]") {
    auto r = beforeCaret("abc", 3, "c");
    REQUIRE(r.has_value());
    REQUIRE(*r == "abc");
}

TEST_CASE("beforeCaret rejects a caret past the end", "[caret_context]") {
    REQUIRE_FALSE(beforeCaret("abc", 4, "c").has_value());
}

TEST_CASE("beforeCaret rejects text missing the last keystroke", "[caret_context]") {
    // App still reports the text from before the user typed "x".
    REQUIRE_FALSE(beforeCaret("abc", 3, "x").has_value());
    REQUIRE_FALSE(beforeCaret("", 0, "x").has_value());
}

TEST_CASE("lastChar returns the final code point", "[caret_context]") {
    REQUIRE(lastChar("") == "");
    REQUIRE(lastChar("ab") == "b");
    REQUIRE(lastChar("aç") == "ç");
    REQUIRE(lastChar("€") == "€");
}

TEST_CASE("trimLeadingBlanksAfter drops the space after a trailing space", "[caret_context]") {
    std::string s = " relevant suggestions";
    trimLeadingBlanksAfter(s, "suggest ");
    REQUIRE(s == "relevant suggestions");

    std::string t = "  \tnext";
    trimLeadingBlanksAfter(t, "line\n");
    REQUIRE(t == "next");
}

TEST_CASE("trimLeadingBlanksAfter keeps spacing when the context ends in a word", "[caret_context]") {
    std::string s = " relevant";
    trimLeadingBlanksAfter(s, "are not");
    REQUIRE(s == " relevant");

    std::string mid = "ustrated";
    trimLeadingBlanksAfter(mid, "i feel f");
    REQUIRE(mid == "ustrated");
}

TEST_CASE("trimLeadingBlanksAfter can empty an all-blank token", "[caret_context]") {
    std::string s = " ";
    trimLeadingBlanksAfter(s, "word ");
    REQUIRE(s.empty());
}

TEST_CASE("promptFor drops trailing spaces and tabs only", "[caret_context]") {
    REQUIRE(promptFor("out of the ") == "out of the");
    REQUIRE(promptFor("a \t ") == "a");
    REQUIRE(promptFor("Best regards,\n") == "Best regards,\n");
    REQUIRE(promptFor("environ") == "environ");
    REQUIRE(promptFor("   ") == "");
}

TEST_CASE("withoutPromptEcho cuts a repeated prompt", "[caret_context]") {
    // Qwen3 in Ollama returns the prefill, then the continuation.
    REQUIRE(withoutPromptEcho("Best regards,\nEwerton", "Best regards,\n") == "Ewerton");
    REQUIRE(withoutPromptEcho("when it expired", "when it") == " expired");
}

TEST_CASE("withoutPromptEcho hides a partial echo while it streams", "[caret_context]") {
    REQUIRE(withoutPromptEcho("Best reg", "Best regards,\n") == "");
    REQUIRE(withoutPromptEcho("Best regards,\n", "Best regards,\n") == "");
}

TEST_CASE("withoutPromptEcho leaves a plain continuation alone", "[caret_context]") {
    REQUIRE(withoutPromptEcho(" office", "I was out of the") == " office");
    REQUIRE(withoutPromptEcho("ment", "staging environ") == "ment");
    REQUIRE(withoutPromptEcho("anything", "") == "anything");
}
