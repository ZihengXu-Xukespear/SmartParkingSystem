// =============================================================================
//  SmartParkingSystem - time_utils.cpp
// =============================================================================
#include "time_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace sps::util {

time_point now() { return std::chrono::system_clock::now(); }

std::int64_t now_unix_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               now().time_since_epoch()).count();
}

std::int64_t now_unix_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now().time_since_epoch()).count();
}

time_point from_unix_seconds(std::int64_t secs) {
    return time_point(std::chrono::seconds(secs));
}

std::int64_t to_unix_seconds(time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
               tp.time_since_epoch()).count();
}

std::string format_time(time_point tp, std::string_view fmt) {
    if (fmt.empty()) fmt = "%Y-%m-%d %H:%M:%S";
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[128];
    std::size_t n = std::strftime(buf, sizeof(buf), std::string(fmt).c_str(), &tmv);
    return n ? std::string(buf, n) : std::string();
}

std::optional<time_point> parse_datetime(std::string_view input) {
    if (input.empty()) return std::nullopt;
    std::string s(input);
    // Normalize "T" separator and trailing timezone designators
    for (auto& c : s) if (c == 'T') c = ' ';
    // Drop trailing "Z" or "+HH:MM" if present
    if (!s.empty() && (s.back() == 'Z' || s.back() == 'z')) s.pop_back();
    auto plus = s.find_last_of('+');
    auto minus = s.find_last_of('-');
    std::size_t cut = std::string::npos;
    if (plus != std::string::npos && plus > 10) cut = plus;
    else if (minus != std::string::npos && minus > 10) cut = minus;
    if (cut != std::string::npos) s = s.substr(0, cut);
    int Y, M, D, h, m, sec;
    int consumed = std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d",
                               &Y, &M, &D, &h, &m, &sec);
    if (consumed < 3) return std::nullopt;
    if (consumed < 4) h = 0;
    if (consumed < 5) m = 0;
    if (consumed < 6) sec = 0;
    std::tm tmv{};
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = sec;
    tmv.tm_isdst = -1;
    std::time_t t = std::mktime(&tmv);
    if (t == -1) return std::nullopt;
    return std::chrono::system_clock::from_time_t(t);
}

std::string humanize_duration(std::chrono::seconds dur, bool compact) {
    if (dur.count() < 0) dur = std::chrono::seconds(0);
    std::int64_t total = dur.count();
    std::int64_t days = total / 86400;
    std::int64_t hours = (total % 86400) / 3600;
    std::int64_t mins = (total % 3600) / 60;
    std::ostringstream os;
    if (days > 0) { os << days << "天"; }
    else if (!compact) { /* no day component */ }
    if (hours > 0 || days > 0) os << hours << "小时";
    os << mins << "分";
    return os.str();
}

std::string humanize_relative(time_point tp, time_point now_tp) {
    if (tp == epoch()) return "-";
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(now_tp - tp).count();
    if (secs < 0) {
        return format_time(tp);
    }
    if (secs < 30)        return "刚刚";
    if (secs < 3600)      return std::to_string(secs / 60) + "分钟前";
    if (secs < 86400)     return std::to_string(secs / 3600) + "小时前";
    if (secs < 86400 * 7) return std::to_string(secs / 86400) + "天前";
    return format_time(tp);
}

time_point tm_to_time_point(const std::tm& tm, bool use_local) {
    std::tm copy = tm;
    std::time_t t;
    if (use_local) {
        t = std::mktime(&copy);
    } else {
#ifdef _WIN32
        // MSVC has no timegm(); emulate via the inverse of gmtime_s.
        std::tm scratch{};
        std::time_t probe = 0;
        gmtime_s(&scratch, &probe);
        std::tm utc = tm;
        // Compute the timezone offset that mktime would apply, then un-apply it.
        std::tm local = tm;
        std::time_t local_t = std::mktime(&local);
        std::tm local_back{};
        localtime_s(&local_back, &local_t);
        std::time_t offset = local_t - std::mktime(&local_back);
        t = std::mktime(&utc) - offset;
#else
        t = timegm(&copy);
#endif
    }
    if (t == -1) return epoch();
    return std::chrono::system_clock::from_time_t(t);
}

std::tm time_point_to_tm(time_point tp, bool use_local) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmv{};
    if (use_local) {
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
    } else {
#ifdef _WIN32
        gmtime_s(&tmv, &t);
#else
        gmtime_r(&t, &tmv);
#endif
    }
    return tmv;
}

bool is_today(time_point tp, time_point now_tp) {
    auto a = time_point_to_tm(tp, true);
    auto b = time_point_to_tm(now_tp, true);
    return a.tm_year == b.tm_year && a.tm_yday == b.tm_yday;
}

std::pair<time_point, time_point> day_bounds(time_point tp, bool use_local) {
    auto tmv = time_point_to_tm(tp, use_local);
    tmv.tm_hour = 0; tmv.tm_min = 0; tmv.tm_sec = 0; tmv.tm_isdst = -1;
    auto start = tm_to_time_point(tmv, use_local);
    return { start, start + std::chrono::hours(24) };
}

static std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

std::optional<time_point> add_interval(time_point tp, int amount,
                                       std::string_view unit) {
    auto u = lower(unit);
    if (u == "s" || u == "sec" || u == "second" || u == "seconds") {
        return tp + std::chrono::seconds(amount);
    }
    if (u == "m" || u == "min" || u == "minute" || u == "minutes") {
        return tp + std::chrono::minutes(amount);
    }
    if (u == "h" || u == "hour" || u == "hours") {
        return tp + std::chrono::hours(amount);
    }
    if (u == "d" || u == "day" || u == "days") {
        return tp + std::chrono::hours(24 * amount);
    }
    if (u == "w" || u == "week" || u == "weeks") {
        return tp + std::chrono::hours(24 * 7 * amount);
    }
    if (u == "month" || u == "months") {
        auto tmv = time_point_to_tm(tp, true);
        tmv.tm_mon += amount;
        return tm_to_time_point(tmv, true);
    }
    if (u == "y" || u == "year" || u == "years") {
        auto tmv = time_point_to_tm(tp, true);
        tmv.tm_year += amount;
        return tm_to_time_point(tmv, true);
    }
    return std::nullopt;
}

std::optional<std::int64_t> diff_in_unit(time_point base, time_point tp,
                                          std::string_view unit) {
    auto u = lower(unit);
    if (u == "s" || u == "sec" || u == "second") {
        return std::chrono::duration_cast<std::chrono::seconds>(tp - base).count();
    }
    if (u == "m" || u == "min" || u == "minute") {
        return std::chrono::duration_cast<std::chrono::minutes>(tp - base).count();
    }
    if (u == "h" || u == "hour") {
        return std::chrono::duration_cast<std::chrono::hours>(tp - base).count();
    }
    if (u == "d" || u == "day") {
        return std::chrono::duration_cast<std::chrono::hours>(tp - base).count() / 24;
    }
    return std::nullopt;
}

void sleep_for(std::chrono::milliseconds ms) {
    std::this_thread::sleep_for(ms);
}

std::string format_rfc1123(time_point tp) {
    static const char* day_names[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* month_names[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                        "Jul","Aug","Sep","Oct","Nov","Dec"};
    auto tmv = time_point_to_tm(tp, false);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
                  day_names[tmv.tm_wday], tmv.tm_mday,
                  month_names[tmv.tm_mon], tmv.tm_year + 1900,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return std::string(buf);
}

}  // namespace sps::util