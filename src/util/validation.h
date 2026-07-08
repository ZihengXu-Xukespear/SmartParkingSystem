// =============================================================================
//  SmartParkingSystem - validation.h
//  Input validators for user-submitted form data. All helpers are pure
//  (no I/O, no global state) and reject empty input. They accept std::string
//  and std::string_view interchangeably.
// =============================================================================
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <optional>

namespace sps::util {

// Generic text checks
bool is_blank(std::string_view input);
bool is_within_length(std::string_view input, std::size_t min_len, std::size_t max_len);
bool matches_pattern(std::string_view input, std::string_view regex_src);
bool is_email(std::string_view input);
bool is_url(std::string_view input);
bool is_uuid(std::string_view input);

// Chinese-context validators
bool is_chinese_mobile(std::string_view input);          // 11 digits starting with 1[3-9]
bool is_chinese_id_card(std::string_view input);         // 18 digits, last may be X
bool is_chinese_name(std::string_view input);            // 2-20 CJK chars
bool is_chinese_plate(std::string_view input);           // e.g. 京A12345
bool is_chinese_plate_loose(std::string_view input);     // accepts trailers (新能源)

// Latin/text validators
bool is_ascii_identifier(std::string_view input, std::size_t max_len = 64);
bool is_safe_filename(std::string_view input);
bool is_safe_path_component(std::string_view input);

// Numeric validators
bool is_integer_in_range(std::string_view input, long min_v, long max_v);
bool is_positive_integer(std::string_view input);
bool is_money_string(std::string_view input);            // up to 2 decimals, >= 0
bool is_percentage(std::string_view input);              // 0..100, 0..2 decimals

// Username & password policy helpers
bool is_valid_username(std::string_view input);          // 4..32 [a-zA-Z0-9_]
bool is_strong_password(std::string_view input,
                        std::size_t min_len = 8);        // >= 8, mixed classes
int  password_strength(std::string_view input);          // 0..4 score

// Pagination & query parameter bounds
bool is_safe_pagination(int page, int page_size);
std::vector<std::string> find_validation_errors(
    const std::vector<std::pair<std::string, std::string>>& fields);

}  // namespace sps::util