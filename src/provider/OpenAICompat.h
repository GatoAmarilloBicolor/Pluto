#pragma once

#include "Provider.h"

namespace pluto {

// OpenAI-compatible chat/completions provider. Works with:
//   - Ollama      (http://localhost:11434)
//   - OpenRouter   (https://openrouter.ai/api)
//   - LM Studio   (http://localhost:1234)
//   - Any server implementing POST /v1/chat/completions with SSE streaming.
//
// Tool calls use the OpenAI function-calling format and are mapped
// transparently to/from pluto's ContentBlock types.
class OpenAICompatProvider : public Provider {
public:
	OpenAICompatProvider(std::string api_key, std::string base_url);
	void stream(const CompletionRequest& req, StreamCallback cb) override;
	std::string name() const override { return "openai-compat"; }

private:
	void _StreamImageGen(const CompletionRequest& req, StreamCallback cb);

	std::string fApiKey;
	std::string fBaseUrl;
};

}  // namespace pluto
