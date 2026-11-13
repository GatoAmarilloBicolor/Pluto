// ProviderBase - Shared base implementations for Provider methods.
// Eliminates duplicate complete_sync and estimate_tokens across providers.

#pragma once

#include "Provider.h"
#include <string>

namespace pluto {

// Base class providing default implementations for common Provider methods.
// Derived classes can override as needed.
class ProviderBase : public Provider {
public:
    ~ProviderBase() override {
        if (fCurl) curl_easy_cleanup(fCurl);
    }

    // Default synchronous completion: collects all TEXT_DELTA events.
    std::string complete_sync(const CompletionRequest& req) override {
        std::string result;
        stream(req, [&](const StreamEvent& ev) {
            if (ev.type == StreamEvent::TEXT_DELTA) {
                result += ev.text;
            } else if (ev.type == StreamEvent::ERROR) {
                result = "Error: " + ev.error;
            }
        });
        return result;
    }

    // Default token estimation: ~4 chars per token.
    int estimate_tokens(const std::string& text) const override {
        return static_cast<int>(text.size() / 4) + 1;
    }

    // Default complete() delegates to stream().
    void complete(const CompletionRequest& req, StreamCallback cb) override {
        stream(req, std::move(cb));
    }

protected:
    // Get (or create) a persistent CURL handle for the given URL's host.
    // The handle is reused across stream() calls to avoid repeating the
    // DNS + TCP + TLS handshake on every request. Only one host at a time;
    // if the host changes, the old handle is closed and a new one created.
    CURL* ensure_curl(const std::string& url) {
        std::string host;
        size_t proto = url.find("://");
        if (proto != std::string::npos) {
            size_t start = proto + 3;
            size_t end = url.find_first_of("/?#", start);
            host = (end == std::string::npos) ? url.substr(start)
                                              : url.substr(start, end - start);
        }
        if (host == fCurlHost && fCurl) return fCurl;

        if (fCurl) {
            curl_easy_cleanup(fCurl);
            fCurl = nullptr;
        }
        fCurl = curl_easy_init();
        fCurlHost = host;
        return fCurl;
    }

    // Drop the persistent handle; next ensure_curl() will create a fresh one.
    void reset_curl() {
        if (fCurl) {
            curl_easy_cleanup(fCurl);
            fCurl = nullptr;
        }
        fCurlHost.clear();
    }

private:
    CURL* fCurl = nullptr;
    std::string fCurlHost;
};

}  // namespace pluto
