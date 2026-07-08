// =============================================================================
//  SmartParkingSystem - logger.h
//  Lightweight structured logger. Writes one JSON object per line to stdout
//  (or to a configurable sink) and supports log levels, request scoping,
//  and child loggers that carry inherited context fields.
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sps::util {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5,
};

// Convert a level to its lowercase string ("info", "warn", ...). Used both
// inside the JSON envelope and by configuration parsing.
std::string_view level_name(LogLevel lvl);
std::optional<LogLevel> parse_level(std::string_view name);

// Sink interface — implement this to redirect log lines elsewhere (file,
// syslog, ring buffer, ...). The default sink writes one line per call to
// std::cout.
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const std::string& line) = 0;
    virtual void flush() {}
};

// Default sink: writes to std::cout with an endl flush.
class StdoutSink : public LogSink {
public:
    void write(const std::string& line) override;
    void flush() override { std::cout.flush(); }
};

// Stderr sink for warn+error levels.
class StderrSink : public LogSink {
public:
    void write(const std::string& line) override;
    void flush() override { std::cerr.flush(); }
};

// Sink that copies each line to an in-memory ring buffer. Useful for the
// /api/_debug/recent endpoint.
class MemorySink : public LogSink {
public:
    explicit MemorySink(std::size_t capacity = 1024);
    void write(const std::string& line) override;
    std::vector<std::string> snapshot() const;

private:
    mutable std::mutex mu_;
    std::vector<std::string> ring_;
    std::size_t cap_;
};

// Field value variant accepted by the structured logger. Numbers and strings
// are the common cases; nested maps are flattened with a dotted key.
using FieldValue = std::variant<std::string, int, long, double, bool>;
using FieldMap   = std::vector<std::pair<std::string, FieldValue>>;

class Logger {
public:
    Logger(std::string name, LogLevel threshold = LogLevel::Info,
           std::shared_ptr<LogSink> sink = std::make_shared<StdoutSink>());

    // Set the runtime threshold. Messages below this level are dropped
    // before any formatting work is done.
    void set_threshold(LogLevel lvl);
    LogLevel threshold() const { return threshold_; }

    // Replace the sink. The previous sink is not destroyed by this call.
    void set_sink(std::shared_ptr<LogSink> sink) { sink_ = std::move(sink); }

    // Core entry points.
    void log(LogLevel lvl, std::string_view msg, const FieldMap& fields = {});

    // Convenience helpers — accept either inline FieldMap literals or a
    // braced-init list. Variadic templates pick the best overload.
    template <typename... Args>
    void trace(std::string_view msg, Args&&... args) {
        log(LogLevel::Trace, msg, build_fields_(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void debug(std::string_view msg, Args&&... args) {
        log(LogLevel::Debug, msg, build_fields_(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void info(std::string_view msg, Args&&... args) {
        log(LogLevel::Info, msg, build_fields_(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void warn(std::string_view msg, Args&&... args) {
        log(LogLevel::Warn, msg, build_fields_(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void error(std::string_view msg, Args&&... args) {
        log(LogLevel::Error, msg, build_fields_(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void fatal(std::string_view msg, Args&&... args) {
        log(LogLevel::Fatal, msg, build_fields_(std::forward<Args>(args)...));
    }

    // Create a child logger that adds `fields` to every log entry.
    Logger with_fields(const FieldMap& fields) const;

    // Logger is non-copyable (it owns an atomic threshold and a shared sink
    // reference) but movable so `with_fields` can return a child logger.
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    // std::atomic is not movable; we hand-roll the move operators so the
    // threshold value travels with the rest of the state.
    Logger(Logger&& o) noexcept;
    Logger& operator=(Logger&& o) noexcept;

    const std::string& name() const { return name_; }

private:
    template <typename T, typename... Rest>
    static FieldMap build_fields_(T&& first, Rest&&... rest) {
        FieldMap out;
        out.push_back(to_field_(std::forward<T>(first)));
        append_fields_(out, std::forward<Rest>(rest)...);
        return out;
    }
    static FieldMap build_fields_() { return {}; }
    template <typename T, typename... Rest>
    static void append_fields_(FieldMap& out, T&& first, Rest&&... rest) {
        out.push_back(to_field_(std::forward<T>(first)));
        append_fields_(out, std::forward<Rest>(rest)...);
    }
    static void append_fields_(FieldMap&) {}

    static std::pair<std::string, FieldValue> to_field_(const std::pair<std::string, FieldValue>& kv) { return kv; }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, FieldValue>& kv) {
        return { std::string(kv.first), kv.second };
    }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, std::string>& kv) {
        return { std::string(kv.first), kv.second };
    }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, int>& kv) {
        return { std::string(kv.first), kv.second };
    }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, long>& kv) {
        return { std::string(kv.first), kv.second };
    }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, double>& kv) {
        return { std::string(kv.first), kv.second };
    }
    static std::pair<std::string, FieldValue> to_field_(const std::pair<const char*, bool>& kv) {
        return { std::string(kv.first), kv.second };
    }

    std::string name_;
    std::atomic<LogLevel> threshold_;
    std::shared_ptr<LogSink> sink_;
    FieldMap inherited_;
};

// Process-wide root logger. Created on first use; access via `root_logger`.
Logger& root_logger();

// Convenience wrappers around the root logger.
template <typename... Args>
void log_info(std::string_view msg, Args&&... args)  { root_logger().info(msg, std::forward<Args>(args)...); }
template <typename... Args>
void log_warn(std::string_view msg, Args&&... args)  { root_logger().warn(msg, std::forward<Args>(args)...); }
template <typename... Args>
void log_error(std::string_view msg, Args&&... args) { root_logger().error(msg, std::forward<Args>(args)...); }
template <typename... Args>
void log_debug(std::string_view msg, Args&&... args) { root_logger().debug(msg, std::forward<Args>(args)...); }

}  // namespace sps::util