#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ollama_request.h"

static bool contains(const std::string& s, const std::string& needle) {
    return s.find(needle) != std::string::npos;
}

TEST_CASE("payload has the system prompt and the context as assistant prefill", "[ollama_request]") {
    InferenceConfig cfg;
    cfg.model = "qwen2.5:3b";
    auto body = buildChatPayload(cfg, "I feel the", "Be an autocomplete engine.");

    CHECK(contains(body, R"("model":"qwen2.5:3b")"));
    CHECK(contains(body, R"({"role":"system","content":"Be an autocomplete engine."})"));
    CHECK(contains(body, R"({"role":"assistant","content":"I feel the"})"));
    CHECK(contains(body, R"("stream":true)"));
}

TEST_CASE("payload carries the sampling options", "[ollama_request]") {
    InferenceConfig cfg;
    cfg.num_predict = 12;
    cfg.temperature = 0.3f;
    cfg.top_p       = 0.9f;
    cfg.stop_tokens = {".", "\n"};
    cfg.threads     = 4;
    auto body = buildChatPayload(cfg, "x", "");

    CHECK(contains(body, R"("options":{"num_predict":12,"temperature":0.3,"top_p":0.9,"stop":[".","\n"],"num_thread":4})"));
}

TEST_CASE("context is JSON-escaped", "[ollama_request]") {
    InferenceConfig cfg;
    auto body = buildChatPayload(cfg, "say \"hi\"\nC:\\path\t", "");
    CHECK(contains(body, R"({"role":"assistant","content":"say \"hi\"\nC:\\path\t"})"));
}
