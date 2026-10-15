#include "ollama_request.h"

#include <charconv>
#include <string_view>

namespace {

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

// std::to_chars is locale-independent — snprintf respects LC_NUMERIC and would
// emit "0,3" instead of "0.3" under European locales, breaking Ollama's JSON parser.
std::string number(float v) {
    char buf[32] = {};
    auto r = std::to_chars(buf, buf + sizeof(buf) - 1, static_cast<double>(v),
                           std::chars_format::general, 6);
    *r.ptr = '\0';
    return buf;
}

std::string options(const InferenceConfig& cfg) {
    std::string stop_arr = "[";
    for (size_t i = 0; i < cfg.stop_tokens.size(); ++i) {
        if (i) stop_arr += ',';
        stop_arr += '"';
        stop_arr += json_escape(cfg.stop_tokens[i]);
        stop_arr += '"';
    }
    stop_arr += ']';

    std::string out = R"("options":{"num_predict":)" + std::to_string(cfg.num_predict)
         + R"(,"temperature":)" + number(cfg.temperature)
         + R"(,"top_p":)"      + number(cfg.top_p)
         + R"(,"stop":)"       + stop_arr;
    if (cfg.threads.has_value())
        out += R"(,"num_thread":)" + std::to_string(*cfg.threads);
    out += '}';
    return out;
}

}  // namespace

std::string buildChatPayload(const InferenceConfig& cfg,
                             const std::string&     context,
                             const std::string&     systemPrompt) {
    // Assistant-prefill: context is placed in an incomplete assistant turn.
    // The model continues its own text, bypassing the chat-response pattern that
    // fires when conversational text appears in the user role.
    return std::string(R"({"model":")") + json_escape(cfg.model)
         + R"(","messages":[)"
         + R"({"role":"system","content":")"    + json_escape(systemPrompt) + R"("},)"
         + R"({"role":"assistant","content":")" + json_escape(context) + R"("})"
         + R"(],"stream":true,)" + options(cfg) + "}";
}
