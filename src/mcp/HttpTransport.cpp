#include "HttpTransport.h"
#include "../version.h"
#include "curl_security.h"

#include <curl/curl.h>

#include <string>
#include <utility>
#include <cstdio>

namespace pluto {

namespace {

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}

}  // namespace

HttpTransport::HttpTransport(std::string url, std::map<std::string, std::string> headers)
	: fUrl(std::move(url)), fHeaders(std::move(headers)) {}

std::string HttpTransport::post(const std::string& body, std::string& err) {
	CURL* curl = curl_easy_init();
	if (!curl) { err = "curl_easy_init failed"; return {}; }
	CurlGuard curl_guard(curl);
	std::string out;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	if (!headers) { err = "OOM: curl_slist_append failed"; return {}; }
	headers = curl_slist_append(headers, "accept: application/json");
	if (!headers) { curl_slist_free_all(headers); err = "OOM: curl_slist_append failed"; return {}; }
	for (const auto& [k, v] : fHeaders) {
		std::string line = k + ": " + v;
		headers = curl_slist_append(headers, line.c_str());
		if (!headers) { curl_slist_free_all(headers); err = "OOM: curl_slist_append failed"; return {}; }
	}

	curl_easy_setopt(curl, CURLOPT_URL, fUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
	ApplyCurlSecurityWithPinning(curl, fUrl);

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);

	if (rc != CURLE_OK) { err = std::string("curl: ") + curl_easy_strerror(rc); return {}; }
	if (http_code != 200 && http_code != 202) {
		err = "HTTP " + std::to_string(http_code) + ": " + out;
		return {};
	}
	return out;
}

std::string HttpTransport::request(const std::string& body, std::string& err) {
	if (!fCircuitBreaker.allow_request()) {
		err = "Circuit breaker open for " + fUrl;
		return {};
	}

	for (int attempt = 1; attempt <= fRetryPolicy.max_attempts; ++attempt) {
		if (attempt > 1) {
			long delay = calc_backoff_ms(attempt - 1, fRetryPolicy.base_backoff_ms, fRetryPolicy.max_backoff_ms, fRetryPolicy.jitter_factor);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		}

		std::string response = post(body, err);
		if (!response.empty()) {
			fCircuitBreaker.record_success();
			return response;
		}

		// Only retry on transient errors
		bool retryable_curl = false;
		if (err.rfind("curl:", 0) == 0) {
			// Extract curl code from err string — not directly available,
			// but we can check HTTP-level retryability
			retryable_curl = true;
		}
		bool retryable_http = false;
		if (err.rfind("HTTP 429", 0) == 0 ||
		    err.rfind("HTTP 500", 0) == 0 ||
		    err.rfind("HTTP 502", 0) == 0 ||
		    err.rfind("HTTP 503", 0) == 0 ||
		    err.rfind("HTTP 504", 0) == 0) {
			retryable_http = true;
		}

		if (!retryable_curl && !retryable_http) {
			fCircuitBreaker.record_failure();
			return {};
		}

		if (attempt < fRetryPolicy.max_attempts) {
			std::fprintf(stderr, "[mcp/http] Request failed (attempt %d): %s, retrying...\n",
				attempt, err.c_str());
		}
	}

	fCircuitBreaker.record_failure();
	return {};
}

void HttpTransport::notify(const std::string& body) {
	std::string err;
	(void)request(body, err);
}

}  // namespace pluto
