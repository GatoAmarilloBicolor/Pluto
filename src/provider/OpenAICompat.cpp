#include "OpenAICompat.h"

#include <utility>

namespace pluto {

OpenAICompatProvider::OpenAICompatProvider(std::string api_key, std::string base_url)
	: fApiKey(std::move(api_key)), fBaseUrl(std::move(base_url)) {}

void OpenAICompatProvider::stream(const CompletionRequest&, StreamCallback cb) {
	StreamEvent err;
	err.type = StreamEvent::ERROR;
	err.error = "openai-compat provider not yet implemented (Phase 1.5)";
	cb(err);
}

}  // namespace pluto
