#pragma once

#include "ProviderBase.h"
#include "CurlUtil.h"
#include "../token_estimator.h"

namespace pluto {

enum class AuthType { API_KEY, OAUTH };

class AnthropicProvider : public ProviderBase {
public:
    AnthropicProvider(std::string token,
                      AuthType auth_type = AuthType::API_KEY,
                      std::string base_url = "https://api.anthropic.com");
    void stream(const CompletionRequest& req, StreamCallback cb) override;
    bool validate_config(std::string& error_out) const override;
    std::string base_url() const override { return fBaseUrl; }
    void set_base_url(const std::string& url) override { fBaseUrl = url; }
    ProviderCapabilities capabilities() const override;
    std::vector<ModelInfo> list_models() const override;
    bool supports_model(const std::string& model) const override;
    double estimate_cost(const std::string& model, int input_tokens, int output_tokens) const override;
    ProviderType type() const override { return ProviderType::ANTHROPIC; }
    std::string name() const override { return "Anthropic"; }

    // Improved token estimation using model-specific encoder
    int estimate_tokens(const std::string& text) const override {
        return estimate_tokens_for_model(fModel.empty() ? "claude-3-5-sonnet" : fModel, text);
    }

    // Configure retry policy
    void set_retry_policy(const RetryPolicy& policy) { fRetryPolicy = policy; }
    const RetryPolicy& retry_policy() const { return fRetryPolicy; }

    // Set current model for accurate token estimation
    void set_model(const std::string& model) { fModel = model; }

private:
    std::string fToken;
    AuthType    fAuthType;
    std::string fBaseUrl;
    std::string fModel;
    RetryPolicy fRetryPolicy;
    CircuitBreaker fCircuitBreaker;
};

}  // namespace pluto
