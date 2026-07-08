// =============================================================================
//  SmartParkingSystem - logger.cpp
// =============================================================================
#include "logger.h"
#include "string_utils.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <sstream>

namespace sps::util {

std::string_view level_name(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Fatal: return "fatal";
    }
    return "info";
}

std::optional<LogLevel> parse_level(std::string_view name) {
    auto n = to_lower_ascii(name);
    if (n == "trace") return LogLevel::Trace;
    if (n == "debug") return LogLevel::Debug;
    if (n == "info")  return LogLevel::Info;
    if (n == "warn" || n == "warning") return LogLevel::Warn;
    if (n == "error") return LogLevel::Error;
    if (n == "fatal" || n == "critical") return LogLevel::Fatal;
    return std::nullopt;
}

void StdoutSink::write(const std::string& line) {
    std::cout << line << '\n';
}

void StderrSink::write(const std::string& line) {
    std::cerr << line << '\n';
}

MemorySink::MemorySink(std::size_t capacity) : cap_(capacity ? capacity : 1) {
    ring_.reserve(cap_);
}

void MemorySink::write(const std::string& line) {
    std::lock_guard<std::mutex> lock(mu_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(line);
}

std::vector<std::string> MemorySink::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return ring_;
}

namespace {

// Escape a value for inclusion in the JSON envelope. Strings are wrapped in
// double quotes with the standard JSON escapes applied; other types use
// their natural rendering.
std::string render_value(const FieldValue& v) {
    std::ostringstream os;
    std::visit([&](auto&& inner) {
        using T = std::decay_t<decltype(inner)>;
        if constexpr (std::is_same_v<T, std::string>) {
            os << '"' << json_escape(inner) << '"';
        } else if constexpr (std::is_same_v<T, bool>) {
            os << (inner ? "true" : "false");
        } else {
            os << inner;
        }
    }, v);
    return os.str();
}

std::string iso8601_now() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count() % 1000;
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lld",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long long)ms);
    return std::string(buf);
}

}  // namespace

Logger::Logger(std::string name, LogLevel threshold, std::shared_ptr<LogSink> sink)
    : name_(std::move(name)), threshold_(threshold), sink_(std::move(sink)) {}

Logger::Logger(Logger&& o) noexcept
    : name_(std::move(o.name_)),
      threshold_(o.threshold_.load(std::memory_order_relaxed)),
      sink_(std::move(o.sink_)),
      inherited_(std::move(o.inherited_)) {}

Logger& Logger::operator=(Logger&& o) noexcept {
    if (this != &o) {
        name_ = std::move(o.name_);
        threshold_.store(o.threshold_.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
        sink_ = std::move(o.sink_);
        inherited_ = std::move(o.inherited_);
    }
    return *this;
}

void Logger::set_threshold(LogLevel lvl) { threshold_.store(lvl); }

Logger Logger::with_fields(const FieldMap& fields) const {
    Logger out;
    out.name_ = name_;
    out.threshold_.store(threshold_.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    out.sink_ = sink_;
    out.inherited_ = inherited_;
    for (const auto& kv : fields) out.inherited_.push_back(kv);
    return std::move(out);
}

void Logger::log(LogLevel lvl, std::string_view msg, const FieldMap& fields) {
    if (lvl < threshold_.load(std::memory_order_relaxed)) return;

    std::ostringstream os;
    os << '{'
       << "\"ts\":\"" << iso8601_now() << "\","
       << "\"level\":\"" << level_name(lvl) << "\","
       << "\"logger\":\"" << json_escape(name_) << "\","
       << "\"msg\":\"" << json_escape(msg) << "\"";
    for (const auto& kv : inherited_) {
        os << ",\"" << json_escape(kv.first) << "\":" << render_value(kv.second);
    }
    for (const auto& kv : fields) {
        os << ",\"" << json_escape(kv.first) << "\":" << render_value(kv.second);
    }
    os << '}';
    if (sink_) sink_->write(os.str());
}

Logger& root_logger() {
    static Logger inst("root", LogLevel::Info);
    return inst;
}

}  // namespace sps::util