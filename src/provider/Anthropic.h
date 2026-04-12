#pragma once

#include "Provider.h"

namespace pluto {

class AnthropicProvider : public Provider {
public:
	AnthropicProvider(std::string api_key, std::string base_url = "https://api.anthropic.com");
	void stream(const CompletionRequest& req, StreamCallback cb) override;
	std::string name() const override { return "anthropic"; }

private:
	std::string fApiKey;
	std::string fBaseUrl;
};

}  // namespace pluto
