// StreamClient - Shared SSE streaming client with retry logic.
// Simpler utility approach: provides a function that handles curl setup,
// SSE line buffering, and retry logic. Derived classes provide callbacks.

#pragma once

#include "Provider.h"
#include "CurlUtil.h"
#include "curl_security.h"
#include "../version.h"
#include <curl/curl.h>
#include <string>
#include <functional>
#include <atomic>

namespace pluto {

struct StreamClient {
    // Configuration for the stream client
    struct Config {
        std::string base_url;
        std::string user_agent = "Pluto/" PLUTO_VERSION;
        long timeout_seconds = 300;
        long connect_timeout_seconds = 10;
        int max_retries = 3;
        long base_retry_delay_ms = 1000;
        long max_retry_delay_ms = 30000;
    };

    // State for SSE parsing (extended by caller)
    struct State {
        std::string buffer;
        std::atomic<bool>* cancel = nullptr;
        bool done = false;
        bool error = false;
        
        // Callbacks (set by execute)
        std::function<void(const std::string&, State&)> process_line;
        std::function<void(State&)> on_complete;
        std::function<bool(CURL*, State&, int)> on_retry;
        std::function<size_t(char*, size_t, size_t, State*)> write_cb;
    };

    // Callback types
    using PrepareCallback = std::function<void(CURL*, const Config&, HeadersGuard&, const std::string&)>;

    // Execute a streaming POST request with retry logic
    // Returns true on success, false on error (error reported via callback)
    static bool execute(const Config& config,
                        const std::string& body,
                        const PrepareCallback& prepare,
                        State& state);

private:
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int cancel_callback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t);
    static void perform_with_retry(CURL* curl, const Config& config, State& state,
                                   const PrepareCallback& prepare,
                                   const std::string& body);
};

}  // namespace pluto
