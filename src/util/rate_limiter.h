// =============================================================================
//  SmartParkingSystem - rate_limiter.h
//  Token-bucket rate limiter shared across HTTP routes. Buckets are keyed by
//  client IP (or by user id, when authenticated). Limits are configured per
//  bucket; the default is 60 requests / minute which is generous for an
//  in-classroom deployment and tight enough to discourage scraping.
// =============================================================================
#pragma once

#include "cache.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace sps::util {

struct RateLimitConfig {
    double   refill_per_second;   // tokens added per second
    double   burst;               // max bucket size
    std::chrono::milliseconds idle_evict{std::chrono::minutes(10)};
};

struct RateLimitDecision {
    bool      allowed;
    double    tokens_remaining;
    std::chrono::milliseconds retry_after{0};
};

class TokenBucketLimiter {
public:
    explicit TokenBucketLimiter(RateLimitConfig cfg = {}) {
        if (cfg.refill_per_second <= 0) cfg.refill_per_second = 1.0;
        if (cfg.burst <= 0) cfg.burst = cfg.refill_per_second * 4;
        cfg_ = cfg;
    }

    // Try to consume one token from `key`'s bucket. The bucket is created
    // (full) on first use, then refills at `cfg_.refill_per_second`.
    RateLimitDecision try_acquire(const std::string& key,
                                  std::chrono::steady_clock::time_point now
                                      = std::chrono::steady_clock::now()) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = buckets_.find(key);
        if (it == buckets_.end()) {
            Bucket b;
            b.tokens = cfg_.burst;
            b.last_refill = now;
            b.last_used = now;
            buckets_.emplace(key, b);
            return { true, cfg_.burst - 1, std::chrono::milliseconds(0) };
        }
        auto& b = it->second;
        double elapsed = std::chrono::duration<double>(now - b.last_refill).count();
        b.tokens = std::min(cfg_.burst, b.tokens + elapsed * cfg_.refill_per_second);
        b.last_refill = now;
        b.last_used = now;
        if (b.tokens >= 1.0) {
            b.tokens -= 1.0;
            return { true, b.tokens, std::chrono::milliseconds(0) };
        }
        double deficit = 1.0 - b.tokens;
        auto wait_ms = std::chrono::milliseconds(
            static_cast<long long>(deficit / cfg_.refill_per_second * 1000));
        return { false, b.tokens, wait_ms };
    }

    void evict_idle(std::chrono::steady_clock::time_point now
                        = std::chrono::steady_clock::now()) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto it = buckets_.begin(); it != buckets_.end(); ) {
            if (now - it->second.last_used > cfg_.idle_evict) {
                it = buckets_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::size_t bucket_count() const {
        std::lock_guard<std::mutex> lock(mu_);
        return buckets_.size();
    }

private:
    struct Bucket {
        double tokens = 0;
        std::chrono::steady_clock::time_point last_refill{};
        std::chrono::steady_clock::time_point last_used{};
    };

    mutable std::mutex mu_;
    RateLimitConfig cfg_;
    std::unordered_map<std::string, Bucket> buckets_;
};

// Lightweight per-user request quota. Useful for enforcing "no more than 10
// recharge requests per user per hour" type limits.
class PerUserQuota {
public:
    PerUserQuota(int max_events, std::chrono::hours window)
        : max_(max_events), window_(window) {}

    // Returns true when the user is still under quota. Sliding window: events
    // older than `window_` are dropped before counting.
    bool allow(int user_id) {
        auto now = steady_now();
        std::lock_guard<std::mutex> lock(mu_);
        auto& ring = log_[user_id];
        ring.erase(std::remove_if(ring.begin(), ring.end(),
                                  [&](auto t) { return now - t > window_; }),
                   ring.end());
        if ((int)ring.size() >= max_) return false;
        ring.push_back(now);
        return true;
    }

    void reset(int user_id) {
        std::lock_guard<std::mutex> lock(mu_);
        log_.erase(user_id);
    }

private:
    mutable std::mutex mu_;
    int max_;
    std::chrono::hours window_;
    std::unordered_map<int, std::vector<std::chrono::steady_clock::time_point>> log_;
};

}  // namespace sps::util