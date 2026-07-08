// =============================================================================
//  SmartParkingSystem - string_utils.h
//  Internal string manipulation helpers used across services and controllers.
//  All helpers here are UTF-8 aware; length calculations on Chinese text treat
//  each multi-byte character as one logical character when requested.
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <cstdint>

namespace sps::util {

// Remove leading and trailing ASCII whitespace (spaces, tabs, CR/LF, vertical
// tab, form feed). Operates on the raw byte sequence — does NOT collapse
// full-width spaces (　) unless explicitly opted in.
std::string trim(std::string_view input);
std::string trim_left(std::string_view input);
std::string trim_right(std::string_view input);

// Full-width aware trim: also strips 　 (CJK ideographic space) and the
// zero-width space ​, which often leak in from copy-pasted user input.
std::string trim_cjk(std::string_view input);

// Case conversion helpers (ASCII-only; CJK characters are passed through).
std::string to_lower_ascii(std::string_view input);
std::string to_upper_ascii(std::string_view input);

// Returns true when every character in `input` is either ASCII or a
// well-formed UTF-8 continuation byte. Useful for early-rejecting strings
// that contain a stray 0xFF/0xFE byte from a mis-decoded GBK/UTF-16 source.
bool is_valid_utf8(std::string_view input);

// Count visible characters (codepoints) in a UTF-8 string. Differs from
// `input.size()` for any text containing non-ASCII bytes.
std::size_t utf8_length(std::string_view input);

// True when the entire string is composed of ASCII printable characters
// (no control characters, no high bit set).
bool is_ascii_printable(std::string_view input);

// True when the string contains at least one CJK codepoint
// (U+4E00..U+9FFF basic block). Used by plate validators to gate Chinese
// province characters in plate prefixes like "京A12345".
bool contains_cjk(std::string_view input);

// Split a string by delimiter, preserving empty fields when consecutive
// delimiters appear (so "a||b" with "|" yields {"a","","b"}, not {"a","b"}).
std::vector<std::string> split(std::string_view input, char delimiter);
std::vector<std::string> split(std::string_view input, std::string_view delimiter);

// Join a sequence of strings with the given glue. Empty glue concatenates
// without inserting any characters.
std::string join(const std::vector<std::string>& parts, std::string_view glue);

// Replace all occurrences of `from` with `to`. If `from` is empty, the input
// is returned unchanged. Replacement is non-overlapping and left-to-right.
std::string replace_all(std::string_view input,
                        std::string_view from,
                        std::string_view to);

// Replace only the first occurrence of `from` with `to`.
std::string replace_first(std::string_view input,
                          std::string_view from,
                          std::string_view to);

// Case-insensitive substring search. Returns the byte offset of the first
// match, or std::string_view::npos if not found.
std::size_t find_ci(std::string_view haystack, std::string_view needle);

// Returns true when `input` starts with `prefix` (case sensitive by default).
bool starts_with(std::string_view input, std::string_view prefix);
bool ends_with(std::string_view input, std::string_view suffix);
bool starts_with_ci(std::string_view input, std::string_view prefix);
bool ends_with_ci(std::string_view input, std::string_view suffix);

// Convert between std::string and std::wstring using the platform's current
// locale. On Windows this means CP_ACP for `wstring_to_string` and CP_UTF8
// for the `_utf8` variants. Returns std::nullopt on conversion failure.
std::optional<std::wstring> string_to_wstring(std::string_view input);
std::optional<std::string> wstring_to_string(std::wstring_view input);
std::optional<std::wstring> utf8_to_wstring(std::string_view input);
std::optional<std::string> wstring_to_utf8(std::wstring_view input);

// Safe integer parse helpers. Returns std::nullopt on parse failure or when
// the value is outside the supplied [min, max] range. Accepts a leading "+"
// or "-" sign and ignores surrounding whitespace.
std::optional<int>        parse_int(std::string_view input,
                                    int min = INT32_MIN,
                                    int max = INT32_MAX);
std::optional<long>       parse_long(std::string_view input,
                                     long min = LONG_MIN,
                                     long max = LONG_MAX);
std::optional<int64_t>    parse_int64(std::string_view input,
                                      int64_t min = INT64_MIN,
                                      int64_t max = INT64_MAX);
std::optional<double>     parse_double(std::string_view input);

// Format a double to a fixed-precision string (truncating trailing zeros).
// e.g. format_double(3.1400, 2) -> "3.14". Negative `precision` falls back
// to the default %g format.
std::string format_double(double value, int precision = 2);

// Pad a string on the left/right to the desired width using `pad` as filler.
// If `input` is already wider than `width`, it is returned unchanged.
std::string pad_left(std::string_view input, std::size_t width, char pad = ' ');
std::string pad_right(std::string_view input, std::size_t width, char pad = ' ');

// Remove any character appearing in `chars` from both ends of the string.
std::string strip(std::string_view input, std::string_view chars);

// Generate a short random alphanumeric identifier (e.g. for log correlation
// IDs). Uses a thread-local Mersenne Twister seeded once at program start.
std::string random_alnum(std::size_t length);

// Escape a string so it is safe to embed inside a JSON value (handles the
// usual \", \\, \n, \r, \t, \b, \f plus the U+0000..U+001F control range).
std::string json_escape(std::string_view input);

// Escape a string so it is safe to embed inside an HTML text node or
// attribute value. By default escapes ", &, <, >, '.
std::string html_escape(std::string_view input, bool escape_quotes = true);

}  // namespace sps::util