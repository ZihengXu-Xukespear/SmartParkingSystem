// =============================================================================
//  SmartParkingSystem - validation.cpp
// =============================================================================
#include "validation.h"
#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace sps::util {

bool is_blank(std::string_view input) {
    for (char c : input) if (!std::isspace((unsigned char)c)) return false;
    return true;
}

bool is_within_length(std::string_view input, std::size_t min_len, std::size_t max_len) {
    if (input.size() < min_len) return false;
    if (max_len > 0 && input.size() > max_len) return false;
    return true;
}

bool matches_pattern(std::string_view input, std::string_view regex_src) {
    try {
        std::string s(input);
        std::regex re = std::regex(std::string(regex_src));
        return std::regex_match(s.begin(), s.end(), re);
    } catch (...) {
        return false;
    }
}

bool is_email(std::string_view input) {
    static std::regex re = std::regex(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)");
    std::string s(input);
    return std::regex_match(s.begin(), s.end(), re);
}

bool is_url(std::string_view input) {
    static std::regex re = std::regex(R"(^https?://[^\s/$.?#].[^\s]*$)");
    std::string s(input);
    return std::regex_match(s.begin(), s.end(), re);
}

bool is_uuid(std::string_view input) {
    static std::regex re = std::regex(
        R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
    std::string s(input);
    return std::regex_match(s.begin(), s.end(), re);
}

bool is_chinese_mobile(std::string_view input) {
    if (input.size() != 11) return false;
    if (input[0] != '1') return false;
    if (input[1] < '3' || input[1] > '9') return false;
    for (char c : input) if (c < '0' || c > '9') return false;
    return true;
}

bool is_chinese_id_card(std::string_view input) {
    if (input.size() != 18) return false;
    for (std::size_t i = 0; i < 17; ++i) {
        if (input[i] < '0' || input[i] > '9') return false;
    }
    char last = input[17];
    return (last >= '0' && last <= '9') || last == 'X' || last == 'x';
}

bool is_chinese_name(std::string_view input) {
    if (input.empty() || input.size() > 60) return false;
    std::size_t count = utf8_length(input);
    if (count < 2 || count > 20) return false;
    for (auto c : input) {
        // Allow ASCII letters/dots/spaces for transliterated names.
        if (c >= 0x20 && c < 0x80) continue;
        // Else require CJK.
        if (contains_cjk(std::string_view(&c, 1))) continue;
        return false;
    }
    return true;
}

bool is_chinese_plate(std::string_view input) {
    // Province char (CJK) + letter A-Z + 5 alphanumeric
    if (input.size() < 7 || input.size() > 8) return false;
    auto first = std::string(input.substr(0, utf8_length(input) - 6));
    if (!contains_cjk(first)) return false;
    auto tail = input.substr(first.size());
    if (tail.size() != 6) return false;
    if (tail[0] < 'A' || tail[0] > 'Z') return false;
    for (std::size_t i = 1; i < tail.size(); ++i) {
        char c = tail[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

bool is_chinese_plate_loose(std::string_view input) {
    if (is_chinese_plate(input)) return true;
    // Accept 8-char (e.g. 京AD12345) trailers
    if (input.size() < 8 || input.size() > 9) return false;
    auto first = std::string(input.substr(0, utf8_length(input) - 7));
    if (!contains_cjk(first)) return false;
    auto tail = input.substr(first.size());
    if (tail.size() != 7) return false;
    if (tail[0] < 'A' || tail[0] > 'Z') return false;
    for (std::size_t i = 1; i < tail.size(); ++i) {
        char c = tail[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

bool is_ascii_identifier(std::string_view input, std::size_t max_len) {
    if (input.empty() || input.size() > max_len) return false;
    char first = input[0];
    if (!((first >= 'a' && first <= 'z') ||
          (first >= 'A' && first <= 'Z') ||
          first == '_')) return false;
    for (char c : input) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return true;
}

bool is_safe_filename(std::string_view input) {
    if (input.empty() || input.size() > 255) return false;
    for (char c : input) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?'  || c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    return true;
}

bool is_safe_path_component(std::string_view input) {
    if (input.empty() || input.size() > 255) return false;
    for (char c : input) {
        if (c == '/' || c == '\\' || c == '\0') return false;
    }
    return true;
}

bool is_integer_in_range(std::string_view input, long min_v, long max_v) {
    auto v = parse_long(input, min_v, max_v);
    return v.has_value();
}

bool is_positive_integer(std::string_view input) {
    auto v = parse_long(input, 1, LONG_MAX);
    return v.has_value();
}

bool is_money_string(std::string_view input) {
    auto v = parse_double(input);
    if (!v || *v < 0.0) return false;
    auto dot = input.find('.');
    if (dot != std::string_view::npos) {
        std::size_t decimals = input.size() - dot - 1;
        if (decimals > 2) return false;
    }
    return true;
}

bool is_percentage(std::string_view input) {
    auto v = parse_double(input);
    if (!v || *v < 0.0 || *v > 100.0) return false;
    auto dot = input.find('.');
    if (dot != std::string_view::npos) {
        std::size_t decimals = input.size() - dot - 1;
        if (decimals > 2) return false;
    }
    return true;
}

bool is_valid_username(std::string_view input) {
    if (input.size() < 4 || input.size() > 32) return false;
    for (char c : input) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

bool is_strong_password(std::string_view input, std::size_t min_len) {
    if (input.size() < min_len) return false;
    bool has_lower = false, has_upper = false, has_digit = false, has_other = false;
    for (char c : input) {
        if (c >= 'a' && c <= 'z') has_lower = true;
        else if (c >= 'A' && c <= 'Z') has_upper = true;
        else if (c >= '0' && c <= '9') has_digit = true;
        else has_other = true;
    }
    int classes = (int)has_lower + (int)has_upper + (int)has_digit + (int)has_other;
    return classes >= 3;
}

int password_strength(std::string_view input) {
    if (input.empty()) return 0;
    bool has_lower = false, has_upper = false, has_digit = false, has_other = false;
    for (char c : input) {
        if (c >= 'a' && c <= 'z') has_lower = true;
        else if (c >= 'A' && c <= 'Z') has_upper = true;
        else if (c >= '0' && c <= '9') has_digit = true;
        else has_other = true;
    }
    int classes = (int)has_lower + (int)has_upper + (int)has_digit + (int)has_other;
    if (input.size() < 6) return 1;
    if (classes <= 1) return 1;
    if (input.size() < 8) return 2;
    if (classes == 2) return 2;
    if (input.size() < 12) return 3;
    if (classes >= 3) return 4;
    return 3;
}

bool is_safe_pagination(int page, int page_size) {
    if (page < 1 || page > 100000) return false;
    if (page_size < 1 || page_size > 200) return false;
    return true;
}

std::vector<std::string> find_validation_errors(
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::vector<std::string> errs;
    for (auto& [field, rule] : fields) {
        // rule grammar: "<validator>[:<arg>]"
        auto colon = rule.find(':');
        std::string name = rule.substr(0, colon);
        std::string arg = colon == std::string::npos ? std::string() : rule.substr(colon + 1);
        if (name == "non_empty" && is_blank(field))                errs.push_back("empty");
        else if (name == "mobile" && !is_chinese_mobile(field))    errs.push_back("mobile");
        else if (name == "email"  && !is_email(field))             errs.push_back("email");
        else if (name == "plate"  && !is_chinese_plate_loose(field)) errs.push_back("plate");
        else if (name == "name"   && !is_chinese_name(field))      errs.push_back("name");
        else if (name == "idcard" && !is_chinese_id_card(field))   errs.push_back("idcard");
        else if (name == "username" && !is_valid_username(field))  errs.push_back("username");
        else if (name == "url"    && !is_url(field))               errs.push_back("url");
        else if (name == "uuid"   && !is_uuid(field))              errs.push_back("uuid");
    }
    return errs;
}

}  // namespace sps::util