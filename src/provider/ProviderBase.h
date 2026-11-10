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
    ~ProviderBase() override = default;

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
};

}  // namespace pluto
