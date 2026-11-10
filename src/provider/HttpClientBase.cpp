// HttpClientBase.cpp - Shared HTTP client implementation

#include "HttpClientBase.h"
#include "../version.h"
#include <unistd.h>
#include <cstdlib>

namespace pluto {

size_t HttpClientBase::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    StreamState* st = static_cast<StreamState*>(userdata);
    if (!st || st->done) return 0;
    return static_cast<HttpClientBase*>(st->self)->handle_write(ptr, size, nmemb, st);
}

size_t HttpClientBase::handle_write(char* ptr, size_t size, size_t nmemb, StreamState* st) {
    size_t total = size * nmemb;
    st->buffer.append(ptr, total);

    size_t pos = 0;
    while ((pos = st->buffer.find('\n')) != std::string::npos) {
        std::string line = st->buffer.substr(0, pos);
        st->buffer.erase(0, pos + 1);

        if (!line.empty() && line.back() == '\r') line.pop_back();
        process_sse_line(line, *st);
        if (st->cb_error) return 0;
    }
    return total;
}

int HttpClientBase::cancel_callback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto cancel = static_cast<std::atomic<bool>*>(clientp);
    return cancel && cancel->load() ? 1 : 0;
}

void HttpClientBase::stream(const CompletionRequest& req, StreamCallback cb) {
    std::string body;
    try {
        body = build_request_body(req);
    } catch (const std::exception& e) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = std::string("build request failed: ") + e.what();
        cb(err);
        return;
    }

    std::string url = get_base_url();
    if (url.empty()) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "empty base URL";
        cb(err);
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "curl_easy_init failed";
        cb(err);
        return;
    }
    CurlGuard guard(curl);

    HeadersGuard headers;
    add_common_headers(headers, std::string("User-Agent: Pluto/") + PLUTO_VERSION);
    get_auth_headers(headers);

    StreamState st;
    st.cb = std::move(cb);
    st.self = this;  // Store this pointer for callback routing

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);

    // Apply centralized TLS security with certificate pinning
    ApplyCurlSecurityWithPinning(curl, url);

    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, get_timeout_seconds());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, get_connect_timeout_seconds());

    // Cancellation support
    if (req.cancel) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, cancel_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(req.cancel));
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    perform_request(curl, body, st, req);
}

void HttpClientBase::perform_request(CURL* curl, const std::string& body, StreamState& st,
                                     const CompletionRequest& req) {
    int max_retries = get_max_retries();
    long base_delay = get_base_retry_delay_ms();
    long max_delay = get_max_retry_delay_ms();

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        st.buffer.clear();
        st.done = false;
        st.cb_error = false;

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = "Request cancelled";
            st.cb(err);
            return;
        }

        if (res != CURLE_OK) {
            StreamEvent err;
            err.type = StreamEvent::ERROR;
            err.error = std::string("curl: ") + curl_easy_strerror(res);
            if (attempt < max_retries) {
                err.type = StreamEvent::RETRY_INFO;
                err.text = "Network error: " + std::string(curl_easy_strerror(res)) + ", retrying...";
                st.cb(err);
                pluto::sleep_ms(pluto::calc_backoff_ms(attempt, base_delay, max_delay), req.cancel);
                continue;
            }
            st.cb(err);
            return;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            if (!st.done) on_stream_complete(st);
            return;
        }

        // Handle retryable errors
        if (http_code == 429 || (http_code >= 500 && http_code < 600)) {
            long retry_secs = parse_retry_after_header(curl);
            if (retry_secs < 0) {
                retry_secs = pluto::calc_backoff_ms(attempt, base_delay, max_delay) / 1000;
                if (retry_secs < 1) retry_secs = 1;
            }
            if (retry_secs > 60) retry_secs = 60;

            log_warn("provider", "HTTP %ld (retry-after=%ld), body: %.512s",
                     http_code, retry_secs, st.buffer.c_str());
            StreamEvent info;
            info.type = StreamEvent::RETRY_INFO;
            info.text = "HTTP " + std::to_string(http_code)
                + ", retrying in " + std::to_string(retry_secs) + "s...";
            st.cb(info);
            pluto::sleep_ms(retry_secs * 1000, req.cancel);
            continue;
        }

        // Non-retryable error
        StreamEvent err;
        err.type = StreamEvent::ERROR;
        err.error = "HTTP " + std::to_string(http_code) + ": " + st.buffer;
        st.cb(err);
        return;
    }
}

}  // namespace pluto
