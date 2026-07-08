// =============================================================================
//  SmartParkingSystem - config_loader.cpp
// =============================================================================
#include "config_loader.h"
#include "string_utils.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace sps::util {

namespace {

ConfigValue parse_value(std::string_view raw) {
    auto s = trim(raw);
    if (s.empty()) return std::string{};
    // Quoted string: preserve everything between the quotes verbatim
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return std::string(s.substr(1, s.size() - 2));
    }
    // Boolean
    auto lower = to_lower_ascii(s);
    if (lower == "true"  || lower == "yes" || lower == "on")  return true;
    if (lower == "false" || lower == "no"  || lower == "off") return false;
    // Integer
    bool is_int = !s.empty();
    std::size_t start = 0;
    if (s[0] == '+' || s[0] == '-') start = 1;
    for (std::size_t i = start; i < s.size(); ++i) {
        if (!std::isdigit((unsigned char)s[i])) { is_int = false; break; }
    }
    if (is_int) {
        try { return std::stol(std::string(s)); } catch (...) { /* fall through */ }
    }
    // Float
    bool is_float = !s.empty();
    bool seen_dot = false, seen_e = false;
    std::size_t j = start;
    while (j < s.size() && (std::isdigit((unsigned char)s[j]) ||
                            s[j] == '.' || s[j] == 'e' || s[j] == 'E' ||
                            s[j] == '+' || s[j] == '-')) {
        if (s[j] == '.') {
            if (seen_dot) { is_float = false; break; }
            seen_dot = true;
        } else if (s[j] == 'e' || s[j] == 'E') {
            if (seen_e) { is_float = false; break; }
            seen_e = true;
        }
        ++j;
    }
    if (is_float && j == s.size()) {
        try { return std::stod(std::string(s)); } catch (...) { /* fall through */ }
    }
    return std::string(s);
}

}  // namespace

ConfigFile ConfigLoader::parse(std::string_view text) {
    ConfigFile out;
    std::string section = "default";
    out[section] = {};
    std::size_t pos = 0;
    while (pos < text.size()) {
        auto eol = text.find('\n', pos);
        std::string_view line = text.substr(pos,
            eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        if (eol == std::string_view::npos) pos = text.size();
        else pos = eol + 1;
        // Strip inline comments and surrounding whitespace
        std::size_t hash = line.find('#');
        std::size_t semi = line.find(';');
        std::size_t cut = std::string_view::npos;
        if (hash != std::string_view::npos) cut = hash;
        if (semi != std::string_view::npos && (cut == std::string_view::npos || semi < cut)) cut = semi;
        if (cut != std::string_view::npos) line = line.substr(0, cut);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = std::string(trim(line.substr(1, line.size() - 2)));
            if (out.find(section) == out.end()) out[section] = {};
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        std::string key = std::string(trim(line.substr(0, eq)));
        std::string_view value = line.substr(eq + 1);
        out[section][key] = parse_value(value);
    }
    return out;
}

std::optional<ConfigFile> ConfigLoader::load(std::string_view path) {
    std::ifstream in{std::string(path)};
    if (!in) return std::nullopt;
    std::ostringstream buf;
    buf << in.rdbuf();
    return parse(buf.str());
}

std::optional<ConfigValue> ConfigLoader::get(const ConfigFile& cfg,
                                              std::string_view section,
                                              std::string_view key) {
    auto s = cfg.find(std::string(section));
    if (s == cfg.end()) return std::nullopt;
    auto k = s->second.find(std::string(key));
    if (k == s->second.end()) return std::nullopt;
    return k->second;
}

std::string ConfigLoader::get_string(const ConfigFile& cfg,
                                     std::string_view section,
                                     std::string_view key,
                                     std::string_view fallback) {
    auto v = get(cfg, section, key);
    if (!v) return std::string(fallback);
    if (std::holds_alternative<std::string>(*v)) return std::get<std::string>(*v);
    if (std::holds_alternative<bool>(*v))         return std::get<bool>(*v) ? "true" : "false";
    if (std::holds_alternative<long>(*v))         return std::to_string(std::get<long>(*v));
    if (std::holds_alternative<double>(*v))       return std::to_string(std::get<double>(*v));
    return std::string(fallback);
}

long ConfigLoader::get_integer(const ConfigFile& cfg,
                               std::string_view section,
                               std::string_view key,
                               long fallback) {
    auto v = get(cfg, section, key);
    if (!v) return fallback;
    if (std::holds_alternative<long>(*v))   return std::get<long>(*v);
    if (std::holds_alternative<double>(*v)) return (long)std::get<double>(*v);
    if (std::holds_alternative<bool>(*v))   return std::get<bool>(*v) ? 1 : 0;
    try { return std::stol(std::get<std::string>(*v)); } catch (...) { return fallback; }
}

double ConfigLoader::get_float(const ConfigFile& cfg,
                               std::string_view section,
                               std::string_view key,
                               double fallback) {
    auto v = get(cfg, section, key);
    if (!v) return fallback;
    if (std::holds_alternative<double>(*v)) return std::get<double>(*v);
    if (std::holds_alternative<long>(*v))   return (double)std::get<long>(*v);
    if (std::holds_alternative<bool>(*v))   return std::get<bool>(*v) ? 1.0 : 0.0;
    try { return std::stod(std::get<std::string>(*v)); } catch (...) { return fallback; }
}

bool ConfigLoader::get_bool(const ConfigFile& cfg,
                            std::string_view section,
                            std::string_view key,
                            bool fallback) {
    auto v = get(cfg, section, key);
    if (!v) return fallback;
    if (std::holds_alternative<bool>(*v))   return std::get<bool>(*v);
    if (std::holds_alternative<long>(*v))   return std::get<long>(*v) != 0;
    if (std::holds_alternative<double>(*v)) return std::get<double>(*v) != 0.0;
    auto s = to_lower_ascii(std::get<std::string>(*v));
    if (s == "true" || s == "yes" || s == "on" || s == "1") return true;
    if (s == "false" || s == "no" || s == "off" || s == "0") return false;
    return fallback;
}

std::string ConfigLoader::dump(const ConfigFile& cfg) {
    std::ostringstream os;
    for (const auto& [section, kvs] : cfg) {
        if (section != "default") os << '[' << section << "]\n";
        for (const auto& [k, v] : kvs) {
            os << k << '=';
            std::visit([&](auto&& inner) {
                using T = std::decay_t<decltype(inner)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    bool need_quote = inner.find_first_of(" #;,\"'\n") != std::string::npos;
                    if (need_quote) os << '"' << inner << '"';
                    else os << inner;
                } else if constexpr (std::is_same_v<T, bool>) {
                    os << (inner ? "true" : "false");
                } else {
                    os << inner;
                }
            }, v);
            os << '\n';
        }
        os << '\n';
    }
    return os.str();
}

}  // namespace sps::util