// =============================================================================
//  SmartParkingSystem - http_utils.cpp
// =============================================================================
#include "http_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include "encoding.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace sps::util {

std::string client_ip(const crow::request& req) {
    auto xff = req.get_header_value("X-Forwarded-For");
    if (!xff.empty()) {
        auto first = split_header(xff).front();
        if (!first.empty()) return first;
    }
    auto xri = req.get_header_value("X-Real-IP");
    if (!xri.empty()) return trim(xri);
    if (!req.remote_ip_address.empty()) return req.remote_ip_address;
    return "0.0.0.0";
}

std::vector<std::string> split_header(std::string_view header) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos <= header.size()) {
        auto comma = header.find(',', pos);
        auto seg = header.substr(pos,
            comma == std::string_view::npos ? std::string_view::npos : comma - pos);
        auto t = trim(seg);
        if (!t.empty()) out.push_back(t);
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> parse_cookies(std::string_view header) {
    std::vector<std::pair<std::string, std::string>> out;
    std::string h(header);
    auto colon = h.find(':');
    if (colon != std::string::npos) h = h.substr(colon + 1);
    auto parts = split(h, ';');
    for (auto& p : parts) {
        auto t = trim(p);
        if (t.empty()) continue;
        auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        out.emplace_back(trim(t.substr(0, eq)), trim(t.substr(eq + 1)));
    }
    return out;
}

std::string make_etag(std::string_view content, bool weak) {
    auto digest = sha256_hex(content);
    auto short_hash = digest.substr(0, 27);
    std::string out = weak ? std::string("W/\"") + short_hash + "\"" :
                              std::string("\"") + short_hash + "\"";
    return out;
}

std::string status_reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

namespace {

struct MimeEntry { const char* ext; const char* type; };

constexpr MimeEntry kMimeTable[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "application/javascript; charset=utf-8"},
    {".mjs",  "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".xml",  "application/xml; charset=utf-8"},
    {".txt",  "text/plain; charset=utf-8"},
    {".csv",  "text/csv; charset=utf-8"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".mp4",  "video/mp4"},
    {".webm", "video/webm"},
    {".mp3",  "audio/mpeg"},
    {".wav",  "audio/wav"},
    {".woff", "font/woff"},
    {".woff2","font/woff2"},
};

}  // namespace

std::string mime_type(std::string_view path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos) return "application/octet-stream";
    auto ext = path.substr(dot);
    auto lower_ext = to_lower_ascii(ext);
    for (const auto& m : kMimeTable) {
        if (lower_ext == m.ext) return m.type;
    }
    return "application/octet-stream";
}

std::string http_date_now() {
    return format_rfc1123(now());
}

bool accepts(const std::string& accept_header, std::string_view mime) {
    if (accept_header.empty()) return true;
    if (accept_header.find("*/*") != std::string::npos) return true;
    auto main = std::string(mime.substr(0, mime.find('/'))) + "/*";
    if (accept_header.find(main) != std::string::npos) return true;
    return accept_header.find(mime) != std::string::npos;
}

std::optional<std::string> negotiate_content_type(
    const std::string& accept_header,
    const std::vector<std::string>& candidates) {
    for (const auto& c : candidates) {
        if (accepts(accept_header, c)) return c;
    }
    return std::nullopt;
}

crow::response json_ok(crow::json::wvalue payload) {
    crow::json::wvalue envelope;
    envelope["ok"] = true;
    envelope["data"] = std::move(payload);
    crow::response r(envelope);
    r.set_header("Content-Type", "application/json; charset=utf-8");
    return r;
}

crow::response json_error(int http_status,
                          std::string code,
                          std::string message) {
    crow::json::wvalue envelope;
    envelope["ok"] = false;
    envelope["code"] = std::move(code);
    envelope["message"] = std::move(message);
    crow::response r(http_status, envelope);
    r.set_header("Content-Type", "application/json; charset=utf-8");
    return r;
}

}  // namespace sps::util