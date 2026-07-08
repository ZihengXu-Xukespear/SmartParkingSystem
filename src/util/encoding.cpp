// =============================================================================
//  SmartParkingSystem - encoding.cpp
// =============================================================================
#include "encoding.h"
#include "../sha256.h"

#include <cstdint>
#include <cstring>
#include <sstream>

namespace sps::util {

namespace {

constexpr char b64_std_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char b64_url_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

inline int b64_index(char c, bool url_safe) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (url_safe) {
        if (c == '-') return 62;
        if (c == '_') return 63;
    } else {
        if (c == '+') return 62;
        if (c == '/') return 63;
    }
    return -1;
}

inline bool is_hex(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

inline int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline unsigned char from_hex_pair(char hi, char lo) {
    return (unsigned char)((hex_nibble(hi) << 4) | hex_nibble(lo));
}

}  // namespace

std::string base64_encode(std::string_view input, bool url_safe) {
    const char* tbl = url_safe ? b64_url_table : b64_std_table;
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= input.size()) {
        uint32_t v = (uint8_t)input[i] << 16 |
                     (uint8_t)input[i+1] << 8 |
                     (uint8_t)input[i+2];
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        out.push_back(tbl[(v >> 6)  & 0x3F]);
        out.push_back(tbl[v & 0x3F]);
        i += 3;
    }
    if (i < input.size()) {
        uint32_t v = (uint8_t)input[i] << 16;
        if (i + 1 < input.size()) v |= (uint8_t)input[i+1] << 8;
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        if (i + 1 < input.size()) {
            out.push_back(tbl[(v >> 6) & 0x3F]);
        } else if (!url_safe) {
            out.push_back('=');
        }
        if (!url_safe) out.push_back('=');
    }
    return out;
}

std::optional<std::string> base64_decode(std::string_view input, bool url_safe) {
    std::string out;
    out.reserve((input.size() / 4) * 3);
    int buf = 0, bits = 0;
    for (char c : input) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        int v = b64_index(c, url_safe);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((char)((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::string hex_encode(std::string_view input) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0xF]);
    }
    return out;
}

std::optional<std::string> hex_decode(std::string_view input) {
    if (input.size() % 2 != 0) return std::nullopt;
    std::string out;
    out.reserve(input.size() / 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        if (!is_hex(input[i]) || !is_hex(input[i+1])) return std::nullopt;
        out.push_back((char)from_hex_pair(input[i], input[i+1]));
    }
    return out;
}

static inline char hex_from_nibble(int n) {
    return (char)(n < 10 ? '0' + n : 'a' + (n - 10));
}

std::string url_encode_component(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (unsigned char c : input) {
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                          c == '.' || c == '~';
        if (unreserved) {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex_from_nibble(c >> 4));
            out.push_back(hex_from_nibble(c & 0xF));
        }
    }
    return out;
}

std::string url_encode_form(std::string_view input) {
    std::string out = url_encode_component(input);
    auto pos = out.find(' ');
    while (pos != std::string::npos) { out[pos] = '+'; pos = out.find(' ', pos); }
    return out;
}

std::optional<std::string> url_decode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%') {
            if (i + 2 >= input.size()) return std::nullopt;
            if (!is_hex(input[i+1]) || !is_hex(input[i+2])) return std::nullopt;
            out.push_back((char)from_hex_pair(input[i+1], input[i+2]));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string sha256_hex(std::string_view input) {
    return hex_encode(sha256::hash(std::string(input)));
}

std::string sha256_base64(std::string_view input) {
    return base64_encode(sha256::hash(std::string(input)));
}

std::string hmac_sha256(std::string_view key, std::string_view data) {
    // Two-phase construction matches RFC 2104. The block size for SHA-256
    // is 64 bytes; keys longer than that are first hashed.
    constexpr std::size_t B = 64;
    std::string k;
    if (key.size() > B) {
        k = sha256::hash(std::string(key));
    } else {
        k.assign(key);
    }
    k.resize(B, 0);
    std::string ipad(B, '\x36'), opad(B, '\x5c');
    for (std::size_t i = 0; i < B; ++i) {
        ipad[i] = (char)(ipad[i] ^ k[i]);
        opad[i] = (char)(opad[i] ^ k[i]);
    }
    std::string inner = ipad + std::string(data);
    std::string inner_hash = sha256::hash(inner);
    return sha256::hash(opad + inner_hash);
}

bool constant_time_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return diff == 0;
}

std::string byte_reverse(std::string_view input) {
    std::string out(input);
    std::reverse(out.begin(), out.end());
    return out;
}

std::string pack_be16(uint16_t v) {
    return std::string{(char)((v >> 8) & 0xFF), (char)(v & 0xFF)};
}

std::string pack_be32(uint32_t v) {
    return std::string{
        (char)((v >> 24) & 0xFF),
        (char)((v >> 16) & 0xFF),
        (char)((v >> 8)  & 0xFF),
        (char)(v & 0xFF)};
}

std::string pack_be64(uint64_t v) {
    return std::string{
        (char)((v >> 56) & 0xFF),
        (char)((v >> 48) & 0xFF),
        (char)((v >> 40) & 0xFF),
        (char)((v >> 32) & 0xFF),
        (char)((v >> 24) & 0xFF),
        (char)((v >> 16) & 0xFF),
        (char)((v >> 8)  & 0xFF),
        (char)(v & 0xFF)};
}

std::optional<uint16_t> unpack_be16(std::string_view input) {
    if (input.size() != 2) return std::nullopt;
    return ((uint16_t)(uint8_t)input[0] << 8) | (uint8_t)input[1];
}

std::optional<uint32_t> unpack_be32(std::string_view input) {
    if (input.size() != 4) return std::nullopt;
    return ((uint32_t)(uint8_t)input[0] << 24) |
           ((uint32_t)(uint8_t)input[1] << 16) |
           ((uint32_t)(uint8_t)input[2] << 8)  |
           (uint32_t)(uint8_t)input[3];
}

std::optional<uint64_t> unpack_be64(std::string_view input) {
    if (input.size() != 8) return std::nullopt;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | (uint8_t)input[i];
    return v;
}

std::vector<std::pair<std::string, std::string>>
    parse_query_string(std::string_view input) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t pos = 0;
    while (pos <= input.size()) {
        auto amp = input.find('&', pos);
        std::string_view seg = input.substr(pos,
            amp == std::string_view::npos ? std::string_view::npos : amp - pos);
        if (!seg.empty()) {
            auto eq = seg.find('=');
            if (eq == std::string_view::npos) {
                auto k = url_decode(seg);
                if (k) out.emplace_back(*k, std::string());
            } else {
                auto k = url_decode(seg.substr(0, eq));
                auto v = url_decode(seg.substr(eq + 1));
                if (k && v) out.emplace_back(*k, *v);
            }
        }
        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return out;
}

std::string build_query_string(
    const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::string out;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (i) out.push_back('&');
        out += url_encode_form(pairs[i].first);
        if (!pairs[i].second.empty() || true) {
            out.push_back('=');
            out += url_encode_form(pairs[i].second);
        }
    }
    return out;
}

}  // namespace sps::util