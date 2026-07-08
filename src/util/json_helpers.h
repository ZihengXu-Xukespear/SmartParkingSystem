// =============================================================================
//  SmartParkingSystem - json_helpers.h
//  Convenience builders around crow::json::wvalue that produce the response
//  shapes used across the controllers. Centralised so that every endpoint
//  emits the same `{"ok":true,"data":...}` / `{"ok":false,"error":...}`
//  envelope and pagination metadata format.
// =============================================================================
#pragma once

#include "crow.h"
#include <string>
#include <vector>
#include <utility>

namespace sps::util {

// Build a successful response envelope.
//   res["ok"]    = true
//   res["data"]  = payload (passed in by rvalue)
crow::json::wvalue ok_response(crow::json::wvalue payload = crow::json::wvalue());

// Build an error response envelope. `code` is a stable identifier such as
// "permission_denied", "validation_failed", "internal_error". `http_status`
// is not encoded — the controller maps to the HTTP status code separately.
crow::json::wvalue error_response(std::string code,
                                   std::string message,
                                   crow::json::wvalue details = crow::json::wvalue());

// Pagination metadata used by every list endpoint.
crow::json::wvalue pagination(int page, int page_size, std::int64_t total);

// Helper to convert a vector of pair<K,V> into a JSON object.
template <typename K, typename V>
crow::json::wvalue pairs_to_object(const std::vector<std::pair<K, V>>& pairs) {
    crow::json::wvalue obj;
    for (const auto& [k, v] : pairs) {
        obj[std::string(k)] = v;
    }
    return obj;
}

// Helper to convert a vector<T> into a JSON array using a caller-supplied
// converter. Avoids having to write a one-off lambda at every callsite.
template <typename T, typename Fn>
crow::json::wvalue vector_to_array(const std::vector<T>& items, Fn fn) {
    std::vector<crow::json::wvalue> arr;
    arr.reserve(items.size());
    for (const auto& item : items) {
        arr.push_back(fn(item));
    }
    crow::json::wvalue out;
    out = std::move(arr);
    return out;
}

// Build a CSV row from a list of string fields, escaping any embedded quotes
// or newlines. Used by the `/api/vehicle/export` endpoint.
std::string csv_escape(std::string_view field);
std::string csv_row(const std::vector<std::string>& fields);

// Convert a crow::json::rvalue to a wvalue (recursive). Useful when an API
// receives a JSON payload and needs to echo parts of it back to the caller.
crow::json::wvalue rvalue_to_wvalue(const crow::json::rvalue& v);

// Validate that a JSON object contains every key in `required_keys`. Returns
// the first missing key (or empty string when everything is present).
std::string first_missing_key(const crow::json::rvalue& obj,
                              const std::vector<std::string>& required_keys);

// Pretty-print a JSON value with two-space indentation. The result is a
// UTF-8 string suitable for logging or for the `/api/_debug` endpoint.
std::string pretty_print(const crow::json::wvalue& v, int indent = 2);

}  // namespace sps::util