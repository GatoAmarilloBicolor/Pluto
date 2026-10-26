// Dynamic model capability discovery.
// Consults provider /v1/models endpoints to fetch per-model token limits
// instead of relying on hardcoded guesses. Results are cached per-process.

#pragma once

#include <map>
#include <string>
#include <mutex>

namespace pluto {

struct ModelCaps {
	int max_output_tokens = 0;
	int context_window = 0;
	bool known = false;
};

// Look up cached capabilities for a model. Returns {0,0,false} if unknown.
ModelCaps LookupModelCaps(const std::string& provider,
                          const std::string& base_url,
                          const std::string& model);

// Prefetch capabilities for a model (async-friendly; blocks on first call).
void PrefetchModelCaps(const std::string& provider,
                       const std::string& base_url,
                       const std::string& model);

}  // namespace pluto
