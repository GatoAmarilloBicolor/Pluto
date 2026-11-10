// Shared HTTP client base for provider implementations.
// Encapsulates curl handle management, SSE line buffering, retry logic,
// and TLS security to eliminate duplication across providers.

#pragma once

#include "Provider.h"
#include "CurlUtil.h"
#include "curl_security.h"
#include <curl/curl.h>
#include <string>
#include <functional>
#include <vector>
#include <atomic>

namespace pluto {

// Base class for SSE streaming HTTP clients.
// Derived classes must implement:
//   - build_request_body(const CompletionRequest&)
//   - process_sse_line(const std::string&, StreamState&)
//   - get_base_url()
//   - get_auth_headers(HeadersGuard&)
class HttpClientBase {
public:
    HttpClientBase() = default;
    virtual ~HttpClientBase() = default;

    // Main streaming entry point - handles curl setup, retries, cancellation
    void stream(const CompletionRequest& req, StreamCallback cb);

    // Non-streaming completion
    virtual std::string complete_sync(const CompletionRequest& req) = 0;

protected:
    // State for SSE streaming
    struct StreamState {
        std::string buffer;
        StreamCallback cb;
        bool done = false;
        bool cb_error = false;
        HttpClientBase* self = nullptr;  // For callback routing
    };

    // Must be implemented by derived class
    virtual std::string build_request_body(const CompletionRequest& req) const = 0;
    virtual void process_sse_line(const std::string& line, StreamState& st) = 0;
    virtual std::string get_base_url() const = 0;
    virtual void get_auth_headers(HeadersGuard& headers) const = 0;
    virtual long get_timeout_seconds() const { return 300; }
    virtual long get_connect_timeout_seconds() const { return 10; }
    virtual int get_max_retries() const { return 3; }
    virtual long get_base_retry_delay_ms() const { return 1000; }
    virtual long get_max_retry_delay_ms() const { return 30000; }

    // Optional: called after successful response to emit final message_end
    virtual void on_stream_complete(StreamState& st) {}

    // Optional: parse retry-after header
    virtual long parse_retry_after_header(CURL* curl) {
        std::string retry_after_hdr;
        curl_easy_getinfo(curl, CURLINFO_RETRY_AFTER, &retry_after_hdr);
        return pluto::parse_retry_after(retry_after_hdr);
    }

    // Static curl write callback - routes to instance method
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    virtual size_t handle_write(char* ptr, size_t size, size_t nmemb, StreamState* st);

    // Cancellation callback
    static int cancel_callback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t);

private:
    // Perform the actual curl request with retry logic
    void perform_request(CURL* curl, const std::string& body, StreamState& st,
                         const CompletionRequest& req);
};

// Helper for building headers with auth
inline void add_common_headers(HeadersGuard& headers, const std::string& user_agent) {
    headers.append("Content-Type: application/json");
    headers.append("Accept: text/event-stream");
    headers.append(user_agent);
}

}  // namespace pluto
