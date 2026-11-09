// Common libcurl utilities for providers.
// Shared RAII guards and helper functions.

#pragma once

#include <curl/curl.h>
#include <string>
#include <functional>
#include <thread>
#include <chrono>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdlib>

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
		headers = curl_slist_append(headers, header.c_str());
	}
	curl_slist* get() const { return headers; }
};

// Calculate exponential backoff with jitter.
// attempt: 1-based attempt number (first retry is attempt=1)
// base_ms: base delay in milliseconds
// max_ms: maximum delay cap in milliseconds
inline long calc_backoff_ms(int attempt, long base_ms = 1000, long max_ms = 30000) {
	long backoff = base_ms * (1L << (attempt - 1));
	if (backoff > max_ms) backoff = max_ms;
	// Add jitter: ±25%
	long jitter = backoff / 4;
	backoff += (std::rand() % (jitter * 2 + 1)) - jitter;
	if (backoff < 0) backoff = 0;
	return backoff;
}

// Sleep for milliseconds, checking cancellation flag.
inline void sleep_ms(long ms, const std::atomic<bool>* cancel = nullptr) {
#if defined(__HAIKU__) && !defined(__APPLE__)
    snooze(ms * 1000LL);
#else
    if (ms > 0) {
        auto start = std::chrono::steady_clock::now();
        while (ms > 0) {
            if (cancel && cancel->load()) break;
            auto chunk = std::min<long>(ms, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            ms -= chunk;
        }
    }
#endif
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

}  // namespace pluto