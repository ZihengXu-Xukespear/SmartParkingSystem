#pragma once
#include <string>
#include <vector>
#include "crow.h"

// One tool call requested by the model (OpenAI function-calling shape).
struct LlmToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // normalized JSON object string, e.g. "{}" or "{\"plate\":\"...\"}"
};

// Result of a single chat-completion round.
struct LlmResponse {
    std::string content;
    std::vector<LlmToolCall> tool_calls;
    std::string finish_reason;
    std::string error;
    bool ok = false;
};

// Thin wrapper around the Python llm_bridge.py subprocess. The C++ side owns
// the tool-calling loop (tool execution = DB reads in C++); this class only
// performs one completion per call.
class LlmClient {
public:
    // `messages` are OpenAI-shaped message objects (role/content/tool_calls...).
    // `toolsJson` is a raw JSON string for the tools array (kept as a string to
    // avoid fighting the JSON builder with empty objects). May be empty.
    static LlmResponse complete(const std::vector<crow::json::wvalue>& messages,
                                const std::string& toolsJson);
};
