// =============================================================================
//  SmartParkingSystem - string_utils.cpp
//  Implementation of the helpers declared in string_utils.h.
// =============================================================================
#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace sps::util {

namespace {

// Thread-local PRNG seeded from random_device at first use. Reusing one
// generator per thread avoids the contention of a global mutex around
// random_alnum() calls.
thread_local std::mt19937_64 tls_rng([] {
    std::random_device rd;
    return ((uint64_t)rd() << 32) ^ rd() ^
           (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
}());

inline bool is_ascii_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

}  // namespace

std::string trim(std::string_view input) {
    auto begin = input.find_first_not_of(" \t\n\r\v\f");
    if (begin == std::string_view::npos) return {};
    auto end = input.find_last_not_of(" \t\n\r\v\f");
    return std::string(input.substr(begin, end - begin + 1));
}

std::string trim_left(std::string_view input) {
    auto begin = input.find_first_not_of(" \t\n\r\v\f");
    if (begin == std::string_view::npos) return {};
    return std::string(input.substr(begin));
}

std::string trim_right(std::string_view input) {
    auto end = input.find_last_not_of(" \t\n\r\v\f");
    if (end == std::string_view::npos) return {};
    return std::string(input.substr(0, end + 1));
}

std::string trim_cjk(std::string_view input) {
    auto begin = input.find_first_not_of(" \t\n\r\v\f　​");
    if (begin == std::string_view::npos) return {};
    auto end = input.find_last_not_of(" \t\n\r\v\f　​");
    return std::string(input.substr(begin, end - begin + 1));
}

std::string to_lower_ascii(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

std::string to_upper_ascii(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return out;
}

bool is_valid_utf8(std::string_view input) {
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    const auto* end = p + input.size();
    while (p < end) {
        unsigned char c = *p;
        if (c < 0x80) { ++p; continue; }
        int extra = 0;
        unsigned int min_val = 0;
        if      ((c & 0xE0) == 0xC0) { extra = 1; min_val = 0x80;        c &= 0x1F; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; min_val = 0x800;       c &= 0x0F; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; min_val = 0x10000;     c &= 0x07; }
        else return false;
        if (p + extra >= end) return false;
        unsigned int v = c;
        for (int i = 0; i < extra; ++i) {
            unsigned char n = p[i + 1];
            if ((n & 0xC0) != 0x80) return false;
            v = (v << 6) | (n & 0x3F);
        }
        if (v < min_val) return false;
        // Reject UTF-16 surrogates (U+D800..U+DFFF) and values above U+10FFFF.
        if (v >= 0xD800 && v <= 0xDFFF) return false;
        if (v > 0x10FFFF) return false;
        p += 1 + extra;
    }
    return true;
}

std::size_t utf8_length(std::string_view input) {
    std::size_t count = 0;
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    const auto* end = p + input.size();
    while (p < end) {
        unsigned char c = *p;
        if      (c < 0x80)               p += 1;
        else if ((c & 0xE0) == 0xC0)     p += 2;
        else if ((c & 0xF0) == 0xE0)     p += 3;
        else if ((c & 0xF8) == 0xF0)     p += 4;
        else                            ++p;
        ++count;
    }
    return count;
}

bool is_ascii_printable(std::string_view input) {
    for (unsigned char c : input) {
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

bool contains_cjk(std::string_view input) {
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    const auto* end = p + input.size();
    while (p < end) {
        unsigned char c = *p;
        unsigned int v = 0;
        int extra = 0;
        if      (c < 0x80)         { ++p; continue; }
        else if ((c & 0xE0) == 0xC0) { v = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { v = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { v = c & 0x07; extra = 3; }
        else return false;
        if (p + extra >= end) return false;
        for (int i = 0; i < extra; ++i) v = (v << 6) | (p[i + 1] & 0x3F);
        if (v >= 0x4E00 && v <= 0x9FFF) return true;
        p += 1 + extra;
    }
    return false;
}

std::vector<std::string> split(std::string_view input, char delimiter) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (true) {
        auto next = input.find(delimiter, pos);
        if (next == std::string_view::npos) {
            out.emplace_back(input.substr(pos));
            break;
        }
        out.emplace_back(input.substr(pos, next - pos));
        pos = next + 1;
    }
    return out;
}

std::vector<std::string> split(std::string_view input, std::string_view delimiter) {
    std::vector<std::string> out;
    if (delimiter.empty()) {
        out.emplace_back(input);
        return out;
    }
    std::size_t pos = 0;
    while (pos <= input.size()) {
        auto next = input.find(delimiter, pos);
        if (next == std::string_view::npos) {
            out.emplace_back(input.substr(pos));
            break;
        }
        out.emplace_back(input.substr(pos, next - pos));
        pos = next + delimiter.size();
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, std::string_view glue) {
    if (parts.empty()) return {};
    std::string out = parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i) {
        out.append(glue);
        out.append(parts[i]);
    }
    return out;
}

std::string replace_all(std::string_view input,
                        std::string_view from,
                        std::string_view to) {
    if (from.empty()) return std::string(input);
    std::string out;
    out.reserve(input.size());
    std::size_t pos = 0;
    while (pos <= input.size()) {
        auto next = input.find(from, pos);
        if (next == std::string_view::npos) {
            out.append(input.substr(pos));
            break;
        }
        out.append(input.substr(pos, next - pos));
        out.append(to);
        pos = next + from.size();
    }
    return out;
}

std::string replace_first(std::string_view input,
                          std::string_view from,
                          std::string_view to) {
    if (from.empty()) return std::string(input);
    auto pos = input.find(from);
    if (pos == std::string_view::npos) return std::string(input);
    std::string out;
    out.reserve(input.size() + to.size());
    out.append(input.substr(0, pos));
    out.append(to);
    out.append(input.substr(pos + from.size()));
    return out;
}

std::size_t find_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return 0;
    if (needle.size() > haystack.size()) return std::string_view::npos;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            unsigned char a = (unsigned char)haystack[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (std::tolower(a) != std::tolower(b)) { ok = false; break; }
        }
        if (ok) return i;
    }
    return std::string_view::npos;
}

bool starts_with(std::string_view input, std::string_view prefix) {
    return input.size() >= prefix.size() &&
           input.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(std::string_view input, std::string_view suffix) {
    return input.size() >= suffix.size() &&
           input.compare(input.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with_ci(std::string_view input, std::string_view prefix) {
    if (prefix.size() > input.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower((unsigned char)input[i]) !=
            std::tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

bool ends_with_ci(std::string_view input, std::string_view suffix) {
    if (suffix.size() > input.size()) return false;
    std::size_t off = input.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower((unsigned char)input[off + i]) !=
            std::tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

std::optional<std::wstring> string_to_wstring(std::string_view input) {
    if (input.empty()) return std::wstring{};
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    int needed = MultiByteToWideChar(CP_ACP, 0,
                                      reinterpret_cast<const char*>(p),
                                      (int)input.size(), nullptr, 0);
    if (needed <= 0) return std::nullopt;
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_ACP, 0,
                                      reinterpret_cast<const char*>(p),
                                      (int)input.size(),
                                      out.data(), needed);
    if (written <= 0) return std::nullopt;
    return out;
}

std::optional<std::string> wstring_to_string(std::wstring_view input) {
    if (input.empty()) return std::string{};
    int needed = WideCharToMultiByte(CP_ACP, 0,
                                     input.data(), (int)input.size(),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::nullopt;
    std::string out(static_cast<std::size_t>(needed), '\0');
    int written = WideCharToMultiByte(CP_ACP, 0,
                                      input.data(), (int)input.size(),
                                      out.data(), needed, nullptr, nullptr);
    if (written <= 0) return std::nullopt;
    return out;
}

std::optional<std::wstring> utf8_to_wstring(std::string_view input) {
    if (input.empty()) return std::wstring{};
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    int needed = MultiByteToWideChar(CP_UTF8, 0,
                                      reinterpret_cast<const char*>(p),
                                      (int)input.size(), nullptr, 0);
    if (needed <= 0) return std::nullopt;
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0,
                                      reinterpret_cast<const char*>(p),
                                      (int)input.size(),
                                      out.data(), needed);
    if (written <= 0) return std::nullopt;
    return out;
}

std::optional<std::string> wstring_to_utf8(std::wstring_view input) {
    if (input.empty()) return std::string{};
    int needed = WideCharToMultiByte(CP_UTF8, 0,
                                     input.data(), (int)input.size(),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::nullopt;
    std::string out(static_cast<std::size_t>(needed), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0,
                                      input.data(), (int)input.size(),
                                      out.data(), needed, nullptr, nullptr);
    if (written <= 0) return std::nullopt;
    return out;
}

std::optional<int> parse_int(std::string_view input, int min, int max) {
    auto t = trim(input);
    if (t.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        int v = std::stoi(std::string(t), &consumed);
        if (consumed != t.size()) return std::nullopt;
        if (v < min || v > max) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<long> parse_long(std::string_view input, long min, long max) {
    auto t = trim(input);
    if (t.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        long v = std::stol(std::string(t), &consumed);
        if (consumed != t.size()) return std::nullopt;
        if (v < min || v > max) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int64_t> parse_int64(std::string_view input,
                                   int64_t min, int64_t max) {
    auto t = trim(input);
    if (t.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        int64_t v = std::stoll(std::string(t), &consumed);
        if (consumed != t.size()) return std::nullopt;
        if (v < min || v > max) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> parse_double(std::string_view input) {
    auto t = trim(input);
    if (t.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        double v = std::stod(std::string(t), &consumed);
        if (consumed != t.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::string format_double(double value, int precision) {
    if (precision < 0) {
        std::ostringstream os;
        os << value;
        return os.str();
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, value);
    std::string s(buf);
    // Strip trailing zeros but keep at least one digit after the point.
    auto dot = s.find('.');
    if (dot != std::string::npos) {
        auto last = s.find_last_not_of('0');
        if (last != std::string::npos && last > dot) s.erase(last + 1);
    }
    return s;
}

std::string pad_left(std::string_view input, std::size_t width, char pad) {
    if (input.size() >= width) return std::string(input);
    return std::string(width - input.size(), pad) + std::string(input);
}

std::string pad_right(std::string_view input, std::size_t width, char pad) {
    if (input.size() >= width) return std::string(input);
    return std::string(input) + std::string(width - input.size(), pad);
}

std::string strip(std::string_view input, std::string_view chars) {
    if (chars.empty()) return std::string(input);
    auto begin = input.find_first_not_of(chars);
    if (begin == std::string_view::npos) return {};
    auto end = input.find_last_not_of(chars);
    return std::string(input.substr(begin, end - begin + 1));
}

std::string random_alnum(std::size_t length) {
    static constexpr char alphabet[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static constexpr std::size_t N = sizeof(alphabet) - 1;
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) out.push_back(alphabet[dist(tls_rng)]);
    return out;
}

std::string json_escape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (unsigned char c : input) {
        switch (c) {
            case '"':  out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b");  break;
            case '\f': out.append("\\f");  break;
            case '\n': out.append("\\n");  break;
            case '\r': out.append("\\r");  break;
            case '\t': out.append("\\t");  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out.append(buf);
                } else {
                    out.push_back((char)c);
                }
        }
    }
    return out;
}

std::string html_escape(std::string_view input, bool escape_quotes) {
    std::string out;
    out.reserve(input.size() + 8);
    for (unsigned char c : input) {
        switch (c) {
            case '&':  out.append("&amp;");  break;
            case '<':  out.append("&lt;");   break;
            case '>':  out.append("&gt;");   break;
            case '"':
                out.append(escape_quotes ? "&quot;" : "\"");
                break;
            case '\'':
                out.append(escape_quotes ? "&#39;" : "'");
                break;
            default:
                out.push_back((char)c);
        }
    }
    return out;
}

}  // namespace sps::util