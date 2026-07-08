#include "llm_client.h"
#include "../config.h"
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

// ---------- Python discovery (the LLM bridge only needs the stdlib) ----------
static std::vector<std::string> llmPythonCandidates() {
    std::vector<std::string> out;
#ifdef _WIN32
    FILE* fp = _popen("where python 2>nul", "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            std::string s(line);
            s.erase(s.find_last_not_of(" \r\n") + 1);
            if (!s.empty()) out.push_back(s);
        }
        _pclose(fp);
    }
    out.push_back("python");
#else
    out.push_back("python3");
    out.push_back("python");
#endif
    return out;
}

static bool llmPythonCanRun(const std::string& python) {
    std::string cmd = "\"" + python + "\" -c \"import json,urllib.request\"";
#ifdef _WIN32
    cmd += " 2>nul";
    FILE* fp = _popen(cmd.c_str(), "r");
#else
    cmd += " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
#endif
    if (!fp) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) { /* drain */ }
#ifdef _WIN32
    int status = _pclose(fp);
    return status == 0;
#else
    int status = pclose(fp);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static std::string llmFindPython() {
    static std::string resolved;
    static bool done = false;
    if (done) return resolved;
    for (const auto& cand : llmPythonCandidates()) {
        if (llmPythonCanRun(cand)) { resolved = cand; done = true; return resolved; }
    }
    auto cands = llmPythonCandidates();
    resolved = cands.empty() ? "python" : cands.front();
    done = true;
    return resolved;
}

static std::string llmFindBridge() {
    static const char* cand[] = {
        "llm_bridge.py", "../llm_bridge.py",
        "src/llm_bridge.py", "../src/llm_bridge.py",
    };
    for (auto p : cand) {
        std::ifstream f(p);
        if (f.good()) return p;
    }
    return "llm_bridge.py";
}

static std::string llmTempDir() {
#ifdef _WIN32
    char tmpbuf[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpbuf);
    return tmpbuf;
#else
    return "/tmp/";
#endif
}

// Escape a C++ string into a JSON string literal.
static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

LlmResponse LlmClient::complete(const std::vector<crow::json::wvalue>& messages,
                                const std::string& toolsJson) {
    LlmResponse resp;

    // Resolve credentials (env var takes precedence over config file).
    std::string baseUrl = AppConfig::instance().llm_base_url;
    std::string model   = AppConfig::instance().llm_model;
    std::string apiKey  = AppConfig::instance().llm_api_key;
    if (const char* envKey = std::getenv("SP_LLM_KEY")) {
        std::string ek(envKey);
        if (!ek.empty()) apiKey = ek;
    }
    if (baseUrl.empty()) { resp.error = "LLM base_url 未配置"; return resp; }

    // Build messages JSON by dumping each message object (robust vs. the wvalue API).
    std::string messagesJson = "[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i) messagesJson += ",";
        messagesJson += messages[i].dump();
    }
    messagesJson += "]";

    std::string req = "{";
    req += "\"base_url\":" + jsonStr(baseUrl);
    req += ",\"api_key\":" + jsonStr(apiKey);
    req += ",\"model\":" + jsonStr(model);
    req += ",\"temperature\":0.7";
    req += ",\"max_tokens\":1024";
    req += ",\"messages\":" + messagesJson;
    if (!toolsJson.empty()) req += ",\"tools\":" + toolsJson;
    req += "}";

    std::string reqPath = llmTempDir() + "sp_llm_req.json";
    {
        std::ofstream f(reqPath, std::ios::binary);
        if (!f) { resp.error = "无法写入 LLM 临时文件"; return resp; }
        f.write(req.data(), (std::streamsize)req.size());
    }

    std::string python = llmFindPython();
    std::string script = llmFindBridge();
    std::string cmd = python + " -u \"" + script + "\" \"" + reqPath + "\"";

    std::string out;
#ifdef _WIN32
    FILE* fp = _popen(cmd.c_str(), "r");
#else
    FILE* fp = popen(cmd.c_str(), "r");
#endif
    if (fp) {
        char buf[8192];
        while (fgets(buf, sizeof(buf), fp)) out += buf;
#ifdef _WIN32
        _pclose(fp);
#else
        pclose(fp);
#endif
    }
    std::remove(reqPath.c_str());

    if (out.empty()) { resp.error = "AI 服务无响应，请稍后重试"; return resp; }

    auto r = crow::json::load(out);
    if (!r) { resp.error = "AI 响应解析失败"; return resp; }

    if (r.has("error")) {
        resp.error = r["error"].s();
        return resp;
    }

    resp.ok = true;
    resp.content = r.has("content") ? std::string(r["content"].s()) : "";
    resp.finish_reason = r.has("finish_reason") ? std::string(r["finish_reason"].s()) : "";

    if (r.has("tool_calls")) {
        int n = (int)r["tool_calls"].size();
        for (int i = 0; i < n; ++i) {
            auto tc = r["tool_calls"][i];
            LlmToolCall c;
            c.id = tc.has("id") ? std::string(tc["id"].s()) : ("call_" + std::to_string(i));
            auto fn = tc["function"];
            c.name = fn.has("name") ? std::string(fn["name"].s()) : "";
            c.arguments = fn.has("arguments") ? std::string(fn["arguments"].s()) : "{}";
            resp.tool_calls.push_back(std::move(c));
        }
    }

    return resp;
}
