#pragma once

#include "Provider.h"
#include "ProviderBase.h"
#include "CurlUtil.h"
#include "../config.h"
#include "../json.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <cctype>

namespace pluto {

// Represents a selectable provider option with metadata
struct ProviderOption {
    std::string name;           // Short name (e.g., "anthropic", "ollama", "openrouter")
    std::string label;          // Display label (e.g., "Anthropic (Claude)")
    std::string description;    // Longer description
    std::string provider_type;  // "anthropic" or "openai-compat"
    std::string base_url;       // Base URL for openai-compat
    std::string api_key_env;    // Environment variable for API key
    std::vector<ModelInfo> models;  // Available models (if pre-loaded)
    bool requires_key = true;   // Whether API key is required
    bool is_local = false;      // True for local endpoints (Ollama, LM Studio)
    std::vector<std::string> capabilities; // e.g., "streaming", "tools", "vision"
    int default_max_tokens = 4096;
    int default_context_window = 0;
    std::string auth_hint;      // e.g., "ANTHROPIC_API_KEY", "OPENROUTER_API_KEY"
};

// Provider picker - handles interactive and programmatic provider selection
class ProviderPicker {
public:
    // Load all available provider options from .conf files and built-in defaults
    static std::vector<ProviderOption> LoadOptions();

    // Filter options by query string (fuzzy match on name, label, description)
    static std::vector<ProviderOption> FilterOptions(const std::vector<ProviderOption>& options,
                                                      const std::string& query);

    // Interactive CLI picker - returns selected option or nullopt on cancel
    static std::optional<ProviderOption> PickInteractive(const std::vector<ProviderOption>& options,
                                                          const std::string& prompt = "Select provider: ");

    // Non-interactive: find option by name (exact or fuzzy)
    static std::optional<ProviderOption> FindByName(const std::vector<ProviderOption>& options,
                                                     const std::string& name);

    // Apply selected provider option to Config
    static void ApplyToConfig(Config& cfg, const ProviderOption& opt);

    // Fetch models from provider endpoint (for openai-compat)
    static std::vector<ModelInfo> FetchModels(const std::string& base_url, const std::string& api_key_env);

    // Get capabilities display string
    static std::string FormatCapabilities(const ProviderCapabilities& caps);

    // Check if provider has valid credentials
    static bool HasValidCredentials(const ProviderOption& opt);
};

// Quick provider switching for /provider command
struct ProviderSwitchResult {
    bool success = false;
    std::string message;
    std::string new_provider_name;
};

ProviderSwitchResult SwitchProvider(Config& cfg, const std::string& provider_name);

}  // namespace pluto