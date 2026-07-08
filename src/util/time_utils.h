// =============================================================================
//  SmartParkingSystem - time_utils.h
//  Date/time helpers built around std::chrono + the platform localtime/strftime
//  family. All formatting helpers are locale-independent and produce UTF-8
//  output suitable for embedding in JSON responses or HTML attributes.
// =============================================================================
#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

namespace sps::util {

// Strongly-typed wall-clock instant. Backed by std::chrono::system_clock so
// it can be converted to/from MySQL DATETIME without ambiguity.
using time_point = std::chrono::system_clock::time_point;

// Clock origin pinned to 1970-01-01 UTC. Equivalent to system_clock::from_time_t(0).
inline constexpr time_point epoch() {
    return std::chrono::system_clock::time_point{};
}

// Now() helpers — wrappers used in services to keep call sites readable.
time_point now();
std::int64_t now_unix_seconds();
std::int64_t now_unix_millis();

// Format a time_point using strftime-style format string. The format string
// must be ASCII — pass "YYYY-MM-DD HH:MM:SS" for the canonical database form.
// Empty format defaults to "%Y-%m-%d %H:%M:%S".
std::string format_time(time_point tp, std::string_view fmt = "%Y-%m-%d %H:%M:%S");

// Parse a MySQL DATETIME ("2026-07-08 12:34:56") or ISO-8601 string. Returns
// std::nullopt on bad input. Accepts "T" or " " as the date/time separator.
std::optional<time_point> parse_datetime(std::string_view input);

// Convert between MySQL DATETIME and Unix timestamps.
time_point from_unix_seconds(std::int64_t secs);
std::int64_t to_unix_seconds(time_point tp);

// Human-friendly "X小时Y分" / "Y天X小时Z分" representation. Used for parking
// duration columns. `compact` truncates the days component when zero.
std::string humanize_duration(std::chrono::seconds dur, bool compact = false);

// "X分钟前" / "刚刚" / "Y小时前" relative phrasing for chat timestamps. Returns
// the literal string "-" when tp is at the epoch (so nulls render nicely).
std::string humanize_relative(time_point tp, time_point now_tp = now());

// Convert a tm struct (filled by localtime_r or gmtime_r) into a time_point.
// `dst_indicator` is honored: a negative value disables DST handling.
time_point tm_to_time_point(const std::tm& tm, bool use_local = true);
std::tm time_point_to_tm(time_point tp, bool use_local = true);

// Returns true if the calendar date in `tp` matches today (local time).
bool is_today(time_point tp, time_point now_tp = now());

// Returns the [start, end) range of the local-time day containing `tp`.
// Useful for "today's records" queries that need a [from, to) pair.
std::pair<time_point, time_point> day_bounds(time_point tp, bool use_local = true);

// Add an interval specified by an integer amount and a unit string. Recognised
// units (case-insensitive): "s", "sec", "second", "m", "min", "minute", "h",
// "hour", "d", "day", "w", "week", "month", "y", "year". Returns std::nullopt
// for anything else.
std::optional<time_point> add_interval(time_point tp,
                                       int amount,
                                       std::string_view unit);

// Difference between two timestamps, expressed in the requested unit. The
// result is a signed value (positive when tp > base). Returns std::nullopt
// for unknown unit tokens.
std::optional<std::int64_t> diff_in_unit(time_point base, time_point tp,
                                          std::string_view unit);

// Sleep helpers that don't pollute the global signal mask on POSIX.
void sleep_for(std::chrono::milliseconds ms);

// Format a timestamp as RFC 1123 ("Sun, 06 Nov 1994 08:49:37 GMT"). Useful
// when emitting HTTP Last-Modified headers in static file responses.
std::string format_rfc1123(time_point tp);

}  // namespace sps::util