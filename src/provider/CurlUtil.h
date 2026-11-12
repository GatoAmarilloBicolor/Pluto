// Common libcurl utilities for providers.
// Shared RAII guards and helper functions.

#pragma once

#include <curl/curl.h>
#include <string>
#include <functional>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <random>

namespace pluto {

// RAII guard for CURL handle.
struct CurlGuard {
	CURL* curl = nullptr;
	explicit CurlGuard(CURL* c = nullptr) : curl(c) {}
	~CurlGuard() { if (curl) curl_easy_cleanup(curl); }
	
	CurlGuard(const CurlGuard&) = delete;
	CurlGuard& operator=(const CurlGuard&) = delete;
	
	CurlGuard(CurlGuard&& other) noexcept : curl(other.curl) { other.curl = nullptr; }
	CurlGuard& operator=(CurlGuard&& other) noexcept {
		if (this != &other) {
			if (curl) curl_easy_cleanup(curl);
			curl = other.curl;
			other.curl = nullptr;
		}
		return *this;
	}
	
	CURL* get() const { return curl; }
	CURL* release() { CURL* c = curl; curl = nullptr; return c; }
	void reset(CURL* c = nullptr) {
		if (curl && curl != c) curl_easy_cleanup(curl);
		curl = c;
	}
};

// RAII guard for curl_slist headers.
struct HeadersGuard {
	curl_slist* headers = nullptr;
	HeadersGuard() = default;
	~HeadersGuard() { if (headers) curl_slist_free_all(headers); }
	
	HeadersGuard(const HeadersGuard&) = delete;
	HeadersGuard& operator=(const HeadersGuard&) = delete;
	
	HeadersGuard(HeadersGuard&& other) noexcept : headers(other.headers) { other.headers = nullptr; }
	HeadersGuard& operator=(HeadersGuard&& other) noexcept {
		if (this != &other) {
			if (headers) curl_slist_free_all(headers);
			headers = other.headers;
			other.headers = nullptr;
		}
		return *this;
	}
	
	void append(const std::string& header) {
        curl_slist* new_headers = curl_slist_append(headers, header.c_str());
        if (!new_headers) {
            // OOM: curl_slist_append returns NULL on allocation failure
            // Keep existing headers list, log and continue
            return;
        }
        headers = new_headers;
    }
	curl_slist* get() const { return headers; }
};

// Thread-local random number generator for backoff jitter
inline std::mt19937& get_thread_rng() {
    thread_local std::mt19937 rng([](){
        std::random_device rd;
        std::seed_seq seq{rd(), static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()),
                          static_cast<unsigned>(std::hash<std::thread::id>{}(std::this_thread::get_id()))};
        return std::mt19937(seq);
    }());
    return rng;
}

// Retry policy configuration.
struct RetryPolicy {
    int max_attempts = 3;              // Total attempts including first
    long base_backoff_ms = 1000;       // Initial backoff
    long max_backoff_ms = 30000;       // Cap on backoff
    double jitter_factor = 0.25;       // ±25% jitter
    std::vector<int> retry_status_codes = {429, 500, 502, 503, 504}; // HTTP codes to retry
    std::vector<CURLcode> retry_curl_codes = {
        CURLE_OPERATION_TIMEDOUT,
        CURLE_COULDNT_CONNECT,
        CURLE_COULDNT_RESOLVE_HOST,
        CURLE_PARTIAL_FILE,
        CURLE_RECV_ERROR,
        CURLE_SEND_ERROR,
        CURLE_SSL_CONNECT_ERROR,
        CURLE_SSL_CERTPROBLEM
    };
    
    // Check if an HTTP status code should be retried
    bool should_retry_http(long http_code) const {
        return std::find(retry_status_codes.begin(), retry_status_codes.end(), static_cast<int>(http_code)) != retry_status_codes.end();
    }
    
    // Check if a CURLcode should be retried
    bool should_retry_curl(CURLcode code) const {
        return std::find(retry_curl_codes.begin(), retry_curl_codes.end(), code) != retry_curl_codes.end();
    }
};

// Circuit breaker state for a host.
struct CircuitBreaker {
    mutable std::mutex mutex;
    int failures = 0;
    int successes = 0;
    std::chrono::steady_clock::time_point last_failure;
    bool open = false;
    int failure_threshold = 5;      // Open after N consecutive failures
    int success_threshold = 2;      // Close after N consecutive successes
    long reset_timeout_ms = 60000;  // Time before allowing test request (1 min)
    // Track test request in half-open state to prevent thundering herd
    int half_open_requests = 0;
    const int max_half_open = 1;    // Allow only 1 test request at a time
    
    // Record a successful request
    void record_success() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!open) { failures = 0; successes = 0; return; }
        failures = 0;
        last_failure = {};  // Clear last_failure when recovering
        if (++successes >= success_threshold) {
            open = false;
            successes = 0;
            half_open_requests = 0;
        }
    }
    
    // Record a failed request
    void record_failure() {
        std::lock_guard<std::mutex> lock(mutex);
        successes = 0;
        last_failure = std::chrono::steady_clock::now();
        if (open) {
            half_open_requests = 0;  // Reset half-open counter on failure
        }
        if (++failures >= failure_threshold) {
            open = true;
        }
    }
    
    // Check if circuit allows requests (with auto-half-open)
    bool allow_request() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!open) return true;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_failure).count();
        if (elapsed >= reset_timeout_ms) {
            // Half-open: allow one test request at a time
            if (half_open_requests < max_half_open) {
                ++half_open_requests;
                return true;
            }
            return false;
        }
        return false;
    }
    
    // Get current state for logging
    std::string state() const {
        std::lock_guard<std::mutex> lock(mutex);
        if (open) return "OPEN";
        if (failures > 0) return "DEGRADED";
        return "CLOSED";
    }
};

// Calculate exponential backoff with jitter.
// attempt: 1-based attempt number (first retry is attempt=1)
// base_ms: base delay in milliseconds
// max_ms: maximum delay cap in milliseconds
// jitter_factor: fraction for jitter (0.25 = ±25%)
inline long calc_backoff_ms(int attempt, long base_ms = 1000, long max_ms = 30000, double jitter_factor = 0.25) {
    long backoff = base_ms * (1L << (attempt - 1));
    if (backoff > max_ms) backoff = max_ms;
    long jitter = static_cast<long>(backoff * jitter_factor);
    std::uniform_int_distribution<long> dist(-jitter, jitter);
    backoff += dist(get_thread_rng());
    if (backoff < 0) backoff = 0;
    return backoff;
}

// Sleep for milliseconds, checking cancellation flag.
inline void sleep_ms(long ms, const std::atomic<bool>* cancel = nullptr) {
    if (ms > 0) {
        while (ms > 0) {
            if (cancel && cancel->load()) break;
            auto chunk = std::min<long>(ms, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            ms -= chunk;
        }
    }
}

// Parse retry-after header value.
// Returns seconds to wait, or -1 if not parseable.
inline long parse_retry_after(const std::string& value) {
    if (value.empty()) return -1;
    
    // Could be seconds (integer) or HTTP-date (RFC 1123)
    // For simplicity, only handle seconds format
    try {
        long secs = std::stol(value);
        if (secs > 0 && secs <= 300) return secs;
    } catch (...) {}
    return -1;
}

// Log level for request/response logging
enum class LogLevel { NONE, ERROR, WARN, INFO, DEBUG };

// Thread-safe log callback
class LogCallbackManager {
public:
    using Callback = std::function<void(LogLevel, const std::string&)>;
    
    static void set_callback(Callback cb) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_callback = std::move(cb);
    }
    
    static void log(LogLevel level, const std::string& msg) {
        Callback cb;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            cb = s_callback;
        }
        if (cb) {
            cb(level, msg);
        } else if (level <= LogLevel::WARN) {
            // Default to stderr for warnings/errors
            std::fprintf(stderr, "[pluto-http] %s\n", msg.c_str());
        }
    }
    
private:
    static inline std::mutex s_mutex;
    static inline Callback s_callback;
};

// Convenience functions
inline void set_log_callback(LogCallbackManager::Callback cb) {
    LogCallbackManager::set_callback(std::move(cb));
}

inline void log_request(LogLevel level, const std::string& msg) {
    LogCallbackManager::log(level, msg);
}

}  // namespace pluto