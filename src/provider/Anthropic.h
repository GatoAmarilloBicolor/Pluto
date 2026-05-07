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
	std::string name() const override { return "anthropic"; }

private:
	std::string fToken;
	AuthType    fAuthType;
	std::string fBaseUrl;
};

}  // namespace pluto
