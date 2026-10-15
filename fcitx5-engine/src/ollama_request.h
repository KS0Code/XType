#pragma once

// Pure builder for the Ollama request body behind one suggestion: POST
// /api/chat with the system prompt and the typed text as an unfinished
// assistant turn (assistant-prefill), streamed back as
// {"message":{"content":"…"}} lines.

#include <string>

#include "config.h"

std::string buildChatPayload(const InferenceConfig& cfg,
                             const std::string&     context,
                             const std::string&     systemPrompt);
