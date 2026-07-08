// =============================================================================
//  SmartParkingSystem - http_utils.h
//  Helpers for parsing and producing HTTP-level artifacts (header parsing,
//  cookie decoding, ETag generation, IP extraction, content-type mapping).
//  The implementation depends only on the Crow request/response types and
//  standard library primitives.
// =============================================================================
#pragma once

#include "crow.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace sps::util {

// Extract the originating client IP, honouring X-Forwarded-For / X-Real-IP
// when present. Returns "0.0.0.0" when neither header nor remote_endpoint
// is available.
std::string client_ip(const crow::request& req);

// Split a comma-separated header value (e.g. Accept-Language) into trimmed
// non-empty tokens. Trailing whitespace per token is stripped.
std::vector<std::string> split_header(std::string_view header);

// Parse a single "Cookie: name=value; name2=value2" line into pairs. The
// leading "Cookie:" prefix is optional. Cookie values are NOT URL-decoded —
// callers should pass each value through `url_decode` if they need it.
std::vector<std::pair<std::string, std::string>> parse_cookies(std::string_view header);

// Generate a strong ETag value from arbitrary content. The returned string is
// suitable for the `ETag` response header (quoted, includes the weak prefix
// when `weak` is true).
std::string make_etag(std::string_view content, bool weak = false);

// Convert a numeric status code to its canonical reason phrase. Returns the
// standard text for known codes; "Unknown" otherwise.
std::string status_reason(int code);

// MIME-type lookup. Returns "application/octet-stream" for unknown
// extensions. Recognises common web types plus a few parking-specific ones.
std::string mime_type(std::string_view path);

// Build an RFC 7231 date string (e.g. "Tue, 15 Nov 1994 08:12:31 GMT") from
// the current system clock. Used when emitting Last-Modified / Date headers.
std::string http_date_now();

// True when the supplied Accept header includes the supplied MIME type.
// Wildcards ("*/*", "text/*") are honoured.
bool accepts(const std::string& accept_header, std::string_view mime);

// Negotiation helper: pick the most-preferred MIME from a set of candidates
// that the client accepts. Returns std::nullopt when none match.
std::optional<std::string> negotiate_content_type(
    const std::string& accept_header,
    const std::vector<std::string>& candidates);

// Convenience: produce a JSON 200 response with the standard envelope.
crow::response json_ok(crow::json::wvalue payload);

// Convenience: produce a JSON error response with the standard envelope.
// `http_status` selects the HTTP status code; the JSON envelope carries the
// error code/message.
crow::response json_error(int http_status,
                          std::string code,
                          std::string message);

}  // namespace sps::util