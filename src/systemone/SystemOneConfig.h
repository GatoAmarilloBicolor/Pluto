#pragma once

#include <string>
#include <vector>
#include <map>

namespace pluto {
namespace systemone {

// Decision model provider types.
enum class DecisionModelType {
    REMOTE_KEV,    // kev decision server (HTTP)
    REMOTE_LAYA,   // laya decision server (HTTP)
    REMOTE_JEV,    // jev decision server (HTTP)
    LOCAL,         // Local heuristic-based decision model (offline)
    AUTO           // Auto-select best available
};

// Configuration for System One decision engine.
struct SystemOneConfig {
    DecisionModelType model = DecisionModelType::AUTO;
    std::string api_key;              // API key for remote decision servers
    std::string base_url;             // Base URL for remote decision server
    std::string kev_url = "http://localhost:8080/mcp";
    std::string laya_url = "http://localhost:8081/mcp";
    std::string jev_url = "http://localhost:8082/mcp";
    bool enabled = true;
    double default_threshold = 0.5;
    int timeout_secs = 30;
    bool prefer_local = false;        // Prefer local model when available
    bool fallback_to_local = true;    // Fall back to local if remote fails

    // Context window compression settings
    double context_compaction_threshold = 0.85;  // Compact when >85% full
    int context_target_tokens = 4000;            // Target tokens after compaction
    bool auto_compact_enabled = true;            // Auto-compact context window
    int max_context_tokens = 8000;               // Max context tokens before compaction
    int max_summary_tokens = 200;                // Max tokens for summaries
    int max_key_points = 5;                      // Max key points to extract
};

// Load System One config from ~/config/settings/pluto/systemone.conf
SystemOneConfig LoadSystemOneConfig();

// Save System One config to ~/config/settings/pluto/systemone.conf
bool SaveSystemOneConfig(const SystemOneConfig& cfg);

// Returns true if System One is enabled and configured.
bool IsSystemOneEnabled();

// Returns the resolved API key (from config or env var).
std::string ResolveSystemOneApiKey();

// Map DecisionModelType to string name.
std::string DecisionModelTypeToString(DecisionModelType type);
DecisionModelType DecisionModelTypeFromString(const std::string& str);

// List available decision model names for UI dropdown.
std::vector<std::string> ListDecisionModels();

} // namespace systemone
} // namespace pluto
