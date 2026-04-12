#pragma once

#include "Provider.h"

namespace pluto {

// Stub for Phase 1.5. Targets OpenAI-compatible /v1/chat/completions
// endpoints: llama.cpp server, Ollama, LM Studio, vLLM, OpenAI, DeepSeek, etc.
class OpenAICompatProvider : public Provider {
public:
	OpenAICompatProvider(std::string api_key, std::string base_url);
	void stream(const CompletionRequest& req, StreamCallback cb) override;
	std::string name() const override { return "openai-compat"; }

private:
	std::string fApiKey;
	std::string fBaseUrl;
};

}  // namespace pluto
