#pragma once

#include "Provider.h"

namespace pluto {

enum class AuthType { API_KEY, OAUTH };

class AnthropicProvider : public Provider {
public:
    AnthropicProvider(std::string token,
                      AuthType auth_type = AuthType::API_KEY,
                      std::string base_url = "https://api.anthropic.com");
    void stream(const CompletionRequest& req, StreamCallback cb) override;
    std::string complete_sync(const CompletionRequest& req) override;
    bool validate_config(std::string& error_out) const override;
    std::string base_url() const override { return fBaseUrl; }
    void set_base_url(const std::string& url) override { fBaseUrl = url; }
    ProviderCapabilities capabilities() const override;
    std::vector<ModelInfo> list_models() const override;
    bool supports_model(const std::string& model) const override;
    int estimate_tokens(const std::string& text) const override;
    double estimate_cost(const std::string& model, int input_tokens, int output_tokens) const override;
    ProviderType type() const override { return ProviderType::ANTHROPIC; }
    std::string name() const override { return "Anthropic"; }

private:
    std::string fToken;
    AuthType    fAuthType;
    std::string fBaseUrl;
};

}  // namespace pluto