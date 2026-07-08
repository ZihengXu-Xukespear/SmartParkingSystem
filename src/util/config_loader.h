// =============================================================================
//  SmartParkingSystem - config_loader.h
//  INI-style configuration loader. Reads key=value pairs from a text file
//  with [section] headers, comments starting with '#' or ';', and supports
//  string, integer, float, and boolean values. Used by the LLM bridge and
//  for any optional runtime overrides (log level, billing defaults, ...).
// =============================================================================
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sps::util {

// A configuration value as stored in memory. Booleans are normalised to
// "true"/"false" (case-insensitive on input). Integers are stored as long.
using ConfigValue = std::variant<std::string, long, double, bool>;
using ConfigSection = std::map<std::string, ConfigValue>;
using ConfigFile    = std::map<std::string, ConfigSection>;

class ConfigLoader {
public:
    // Load a config file. Returns std::nullopt when the file cannot be
    // opened. Malformed lines are skipped (not treated as fatal).
    static std::optional<ConfigFile> load(std::string_view path);

    // Same, but parse a string buffer. Useful for tests.
    static ConfigFile parse(std::string_view text);

    // Look up a value. Missing keys return std::nullopt.
    static std::optional<ConfigValue> get(const ConfigFile& cfg,
                                           std::string_view section,
                                           std::string_view key);

    // Typed accessors with defaults. Useful for one-off reads.
    static std::string get_string(const ConfigFile& cfg,
                                  std::string_view section,
                                  std::string_view key,
                                  std::string_view fallback = "");
    static long get_integer(const ConfigFile& cfg,
                            std::string_view section,
                            std::string_view key,
                            long fallback = 0);
    static double get_float(const ConfigFile& cfg,
                            std::string_view section,
                            std::string_view key,
                            double fallback = 0.0);
    static bool get_bool(const ConfigFile& cfg,
                         std::string_view section,
                         std::string_view key,
                         bool fallback = false);

    // Render the config back to disk format. Mainly used by tests and the
    // `/api/_debug/config` debug endpoint.
    static std::string dump(const ConfigFile& cfg);
};

}  // namespace sps::util