#pragma once

#include <string>

namespace pluto {

struct Config {
	std::string provider = "anthropic";
	std::string model = "claude-opus-4-5";
	std::string base_url;          // optional, for openai-compat
	std::string api_key_env = "ANTHROPIC_API_KEY";
	std::string system_prompt;
	int max_tokens = 4096;
	// MCP server configuration lives in its own files (.mcp.json etc);
	// see src/mcp/Config.h.
};

// Load from ~/config/settings/pluto. Missing file is not an error; defaults
// are returned. Values from the file override defaults; env vars override
// both via resolve_api_key below.
Config load_config();

// Resolve API key from the env var named in cfg.api_key_env.
std::string resolve_api_key(const Config& cfg);

}  // namespace pluto
