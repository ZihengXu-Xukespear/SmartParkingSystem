// =============================================================================
//  SmartParkingSystem - encoding.h
//  Small encoding/decoding helpers used across HTTP and DB layers. All
//  routines are pure and exception-free (they return std::nullopt on bad
//  input rather than throwing).
// =============================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sps::util {

// Base64 (RFC 4648). `encode` writes with `=` padding; `decode` accepts both
// padded and unpadded inputs. `url_safe` swaps +/ and /_ for URL contexts.
std::string base64_encode(std::string_view input, bool url_safe = false);
std::optional<std::string> base64_decode(std::string_view input,
                                          bool url_safe = false);

// Hex (lowercase, no separators). `decode` rejects odd-length and non-hex
// characters.
std::string hex_encode(std::string_view input);
std::optional<std::string> hex_decode(std::string_view input);

// URL / form encoding. Percent-encodes everything except the unreserved set
// "A-Z a-z 0-9 - _ . ~". `encode_component` is for path/query segments;
// `encode_form` mirrors application/x-www-form-urlencoded (space -> "+").
std::string url_encode_component(std::string_view input);
std::string url_encode_form(std::string_view input);
std::optional<std::string> url_decode(std::string_view input);

// HMAC-SHA256 implementation built directly on top of the FIPS-180 primitives
// declared in <sha256.h>. Used for signing webhook payloads.
std::string hmac_sha256(std::string_view key, std::string_view data);

// Convenience: SHA-256 of an input rendered as either hex or base64.
std::string sha256_hex(std::string_view input);
std::string sha256_base64(std::string_view input);

// Constant-time compare to avoid timing attacks on token checks.
bool constant_time_equal(std::string_view a, std::string_view b);

// Reverse the bytes of a string. Used for endian-swapping binary payloads.
std::string byte_reverse(std::string_view input);

// Pack/unpack helpers for transmitting 16/32/64-bit integers as big-endian
// strings. These are useful when interacting with Java/.NET backends.
std::string pack_be16(uint16_t v);
std::string pack_be32(uint32_t v);
std::string pack_be64(uint64_t v);
std::optional<uint16_t> unpack_be16(std::string_view input);
std::optional<uint32_t> unpack_be32(std::string_view input);
std::optional<uint64_t> unpack_be64(std::string_view input);

// Encode/decode a list of key=value pairs separated by '&'. Used by the
// query-string helpers and by the form-urlencoded parser.
std::vector<std::pair<std::string, std::string>>
    parse_query_string(std::string_view input);
std::string build_query_string(
    const std::vector<std::pair<std::string, std::string>>& pairs);

}  // namespace sps::util