// =============================================================================
//  SmartParkingSystem - cache.h
//  Thread-safe in-memory LRU cache. Used to short-circuit repeated DB
//  lookups for billing rules, parking lot status, and bulletin entries.
//  The implementation is template-based to support any value type whose
//  copy/move semantics are sane.
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace sps::util {

// TTL helpers — `now()` defaults to std::chrono::steady_clock::now() but
// can be injected for tests.
inline std::chrono::steady_clock::time_point steady_now() {
    return std::chrono::steady_clock::now();
}

// Simple LRU cache with per-entry expiration. The "value" is held by value,
// so storing large objects will copy — prefer std::shared_ptr<T> for those.
template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(std::size_t capacity = 256,
                      std::chrono::milliseconds ttl = std::chrono::seconds(60))
        : capacity_(capacity == 0 ? 1 : capacity), ttl_(ttl) {}

    // Returns a copy of the cached value if present and not expired.
    std::optional<Value> get(const Key& k) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = entries_.find(k);
        if (it == entries_.end()) {
            misses_.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
        const auto& entry = it->second;
        if (entry.expired()) {
            lru_.erase(entry.lru_it);
            entries_.erase(it);
            expired_.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
        // Move-to-front.
        lru_.splice(lru_.begin(), lru_, entry.lru_it);
        hits_.fetch_add(1, std::memory_order_relaxed);
        return entry.value;
    }

    void put(const Key& k, Value v,
             std::chrono::milliseconds ttl_override = std::chrono::milliseconds(-1)) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = entries_.find(k);
        if (it != entries_.end()) {
            lru_.erase(it->second.lru_it);
            entries_.erase(it);
        }
        if (entries_.size() >= capacity_) {
            evict_one_locked_();
        }
        auto ttl = ttl_override.count() < 0 ? ttl_ : ttl_override;
        lru_.push_front(k);
        Entry e;
        e.value = std::move(v);
        e.expires_at = steady_now() + ttl;
        e.lru_it = lru_.begin();
        entries_.emplace(k, std::move(e));
    }

    // Erase a single key.
    bool erase(const Key& k) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = entries_.find(k);
        if (it == entries_.end()) return false;
        lru_.erase(it->second.lru_it);
        entries_.erase(it);
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mu_);
        lru_.clear();
        entries_.clear();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mu_);
        return entries_.size();
    }

    std::size_t capacity() const { return capacity_; }

    struct Stats {
        std::uint64_t hits;
        std::uint64_t misses;
        std::uint64_t expired;
        std::uint64_t evictions;
        std::size_t   size;
    };
    Stats stats() const {
        std::lock_guard<std::mutex> lock(mu_);
        return {
            hits_.load(std::memory_order_relaxed),
            misses_.load(std::memory_order_relaxed),
            expired_.load(std::memory_order_relaxed),
            evictions_.load(std::memory_order_relaxed),
            entries_.size()
        };
    }

private:
    struct Entry {
        Value value;
        std::chrono::steady_clock::time_point expires_at;
        typename std::list<Key>::iterator lru_it;
        bool expired() const { return steady_now() >= expires_at; }
    };

    void evict_one_locked_() {
        if (lru_.empty()) return;
        const Key& k = lru_.back();
        entries_.erase(k);
        lru_.pop_back();
        evictions_.fetch_add(1, std::memory_order_relaxed);
    }

    mutable std::mutex mu_;
    std::size_t capacity_;
    std::chrono::milliseconds ttl_;
    std::list<Key> lru_;
    std::unordered_map<Key, Entry> entries_;
    std::atomic<std::uint64_t> hits_{0};
    std::atomic<std::uint64_t> misses_{0};
    std::atomic<std::uint64_t> expired_{0};
    std::atomic<std::uint64_t> evictions_{0};
};

// Fixed-size counter used to throttle repeated calls (e.g. "this user has
// failed login X times in the last Y minutes"). The counter is monotonic
// within a single window and resets when the window expires.
class SlidingCounter {
public:
    explicit SlidingCounter(std::chrono::milliseconds window)
        : window_(window) {}

    // Record an event, return the new count within the current window.
    std::size_t record() {
        auto t = steady_now();
        std::lock_guard<std::mutex> lock(mu_);
        if (t - window_start_ >= window_) {
            window_start_ = t;
            count_ = 0;
        }
        return ++count_;
    }

    std::size_t peek() const {
        std::lock_guard<std::mutex> lock(mu_);
        return count_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mu_);
        window_start_ = steady_now();
        count_ = 0;
    }

private:
    mutable std::mutex mu_;
    std::chrono::milliseconds window_;
    std::chrono::steady_clock::time_point window_start_{steady_now()};
    std::size_t count_{0};
};

}  // namespace sps::util