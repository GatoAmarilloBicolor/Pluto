#pragma once

#include "ProviderBase.h"
#include "CurlUtil.h"
#include "../token_estimator.h"

#include <unordered_set>

namespace pluto {

// OpenAI-compatible chat/completions provider. Works with:
//   - Ollama      (http://localhost:11434)
//   - OpenRouter   (https://openrouter.ai/api)
//   - LM Studio   (http://localhost:1234)
//   - Any server implementing POST /v1/chat/completions with SSE streaming.
//
// Tool calls use the OpenAI function-calling format and are mapped
// transparently to/from pluto's ContentBlock types.
class OpenAICompatProvider : public ProviderBase {
public:
    OpenAICompatProvider(std::string api_key, std::string base_url);
    void stream(const CompletionRequest& req, StreamCallback cb) override;
    bool validate_config(std::string& error_out) const override;
    std::string base_url() const override { return fBaseUrl; }
    void set_base_url(const std::string& url) override { fBaseUrl = url; }
    ProviderCapabilities capabilities() const override;
    std::vector<ModelInfo> list_models() const override;
    bool supports_model(const std::string& model) const override;
    double estimate_cost(const std::string& model, int input_tokens, int output_tokens) const override;
    ProviderType type() const override { return ProviderType::OPENAI_COMPAT; }
    std::string name() const override { return "OpenAI Compatible"; }

    // Improved token estimation using model-specific encoder
    int estimate_tokens(const std::string& text) const override {
        return estimate_tokens_for_model(fModel.empty() ? "gpt-4o" : fModel, text);
    }

    // Configure retry policy
    void set_retry_policy(const RetryPolicy& policy) { fRetryPolicy = policy; }
    const RetryPolicy& retry_policy() const { return fRetryPolicy; }

    // Set current model for accurate token estimation
    void set_model(const std::string& model) { fModel = model; }

private:
    std::string fApiKey;
    std::string fBaseUrl;
    std::string fModel;
    RetryPolicy fRetryPolicy;
    CircuitBreaker fCircuitBreaker;
    // Track models that don't support tools (persists across turns)
    std::unordered_set<std::string> fToolsUnsupportedModels;
};

}  // namespace pluto
