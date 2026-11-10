#include "StreamClient.h"
#include <unistd.h>
#include <cstdlib>

namespace pluto {

size_t StreamClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    State* state = static_cast<State*>(userdata);
    if (!state || state->done) return 0;
    return state->write_cb ? state->write_cb(ptr, size, nmemb, state) : 0;
}

int StreamClient::cancel_callback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto cancel = static_cast<std::atomic<bool>*>(clientp);
    return cancel && cancel->load() ? 1 : 0;
}

bool StreamClient::execute(const Config& config,
                           const std::string& body,
                           const PrepareCallback& prepare,
                           State& state) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    CurlGuard guard(curl);

    // Prepare headers and request
    HeadersGuard headers;
    headers.append("Content-Type: application/json");
    headers.append("Accept: text/event-stream");
    headers.append(config.user_agent);
    prepare(curl, config, headers, body);

    curl_easy_setopt(curl, CURLOPT_URL, config.base_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);

    // Apply centralized TLS security with certificate pinning
    ApplyCurlSecurityWithPinning(curl, config.base_url);

    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config.connect_timeout_seconds);

    // Cancellation support
    if (state.cancel) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, cancel_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(state.cancel));
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    perform_with_retry(curl, config, state, prepare, body);
    return !state.error;
}

void StreamClient::perform_with_retry(CURL* curl, const Config& config, State& state,
                                      [[maybe_unused]] const PrepareCallback& prepare,
                                      [[maybe_unused]] const std::string& body) {
    // Set up the write callback to process lines
    state.write_cb = [](char* ptr, size_t size, size_t nmemb, State* s) -> size_t {
        size_t total = size * nmemb;
        s->buffer.append(ptr, total);

        size_t pos = 0;
        while ((pos = s->buffer.find('\n')) != std::string::npos) {
            std::string line = s->buffer.substr(0, pos);
            s->buffer.erase(0, pos + 1);

            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (s->process_line) s->process_line(line, *s);
            if (s->error) return 0;
        }
        return total;
    };

    for (int attempt = 1; attempt <= config.max_retries; ++attempt) {
        state.buffer.clear();
        state.done = false;
        state.error = false;

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            state.error = true;
            return;
        }

        if (res != CURLE_OK) {
            state.error = true;
            if (attempt < config.max_retries && state.on_retry && state.on_retry(curl, state, attempt)) {
                pluto::sleep_ms(pluto::calc_backoff_ms(attempt, config.base_retry_delay_ms, config.max_retry_delay_ms), state.cancel);
                continue;
            }
            return;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            if (!state.done && state.on_complete) state.on_complete(state);
            return;
        }

        // Handle retryable errors
        if (http_code == 429 || (http_code >= 500 && http_code < 600)) {
            if (attempt < config.max_retries && state.on_retry && state.on_retry(curl, state, attempt)) {
                std::string retry_after_hdr;
                curl_easy_getinfo(curl, CURLINFO_RETRY_AFTER, &retry_after_hdr);
                long retry_secs = pluto::parse_retry_after(retry_after_hdr);
                if (retry_secs < 0) {
                    retry_secs = pluto::calc_backoff_ms(attempt, config.base_retry_delay_ms, config.max_retry_delay_ms) / 1000;
                    if (retry_secs < 1) retry_secs = 1;
                }
                if (retry_secs > 60) retry_secs = 60;

                pluto::sleep_ms(retry_secs * 1000, state.cancel);
                continue;
            }
        }

        state.error = true;
        return;
    }
}

}  // namespace pluto
