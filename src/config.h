#pragma once

#include <memory>
#include <string>
#include <vector>

namespace pluto {

class Provider;  // forward declaration

// A skill that can be triggered by keywords in user messages.
struct SkillEntry {
	std::string name;
	std::string body;                    // SKILL.md content
	std::vector<std::string> triggers;   // keywords that activate this skill
	bool always = false;                 // true if no trigger (always loaded)
};

// A mode configures which tools are available and injects a mode-specific
// system prompt. Modes are loaded from ~/config/settings/pluto/modes/<name>.md
// with YAML-like frontmatter:
//   ---
//   name: review
//   description: Code review — read-only analysis
//   disabled_tools: write_file, edit_file, apply_patch, bash
//   model: claude-sonnet-4-5
//   ---
//   You are in code review mode. Analyze code without modifying anything.
struct ModeEntry {
	std::string name;
	std::string description;
	std::string prompt;                          // body after frontmatter
	std::string model;                           // optional model override
	std::vector<std::string> disabled_tools;     // tools to hide in this mode
};

struct Config {
	std::string provider = "anthropic";
	std::string model = "claude-opus-4-5";
	std::string base_url;          // optional, for openai-compat
	std::string api_key_env = "ANTHROPIC_API_KEY";
	std::string system_prompt;         // composite (user + memory + skills + mcp)
	std::string user_system_prompt;    // user-editable base, saved to config file
	int max_tokens = 4096;
	int context_window = 200000;   // size of model's context, for the gauge
	bool cache = true;             // ephemeral cache_control on system+tools
	int font_delta = 0;            // GUI font zoom delta, persisted
	bool dry_run = false;          // skip API call, emit a preview of the request
	int max_iterations = 25;       // max agent loop iterations per turn
	std::string active_mode;       // current mode name, empty = default (all tools)
	std::string color_scheme = "auto";  // "auto" (system), "dark", "light"

	// Available modes (built-in + loaded from modes/ dir).
	std::vector<ModeEntry> modes;

	// Hooks: shell commands fired around each tool call. Configured via
	// repeating `hook_pre_tool=` / `hook_post_tool=` lines in the config
	// file. Each invocation gets PLUTO_TOOL=<name> in the environment.
	std::vector<std::string> hooks_pre_tool;
	std::vector<std::string> hooks_post_tool;
	std::vector<std::string> hooks_user_prompt;  // fired before each user turn

	// Triggered skills: loaded on demand when user message matches keywords.
	std::vector<SkillEntry> triggered_skills;

	// MCP server configuration lives in its own files (.mcp.json etc);
	// see src/mcp/Config.h.
};

// A model entry from a .conf provider profile.
struct ProviderConfModel {
	std::string label;
	std::string id;
	int         max_tokens     = 4096;
	int         context_window = 128000;
};

// A provider profile loaded from data/providers/*.conf or
// ~/config/settings/pluto/providers/*.conf.
struct ProviderConf {
	std::string label;
	std::string base_url;
	std::string api_key_env;
	std::vector<ProviderConfModel> models;
};

// Parse a single .conf file into a ProviderConf. Returns false on failure.
bool ParseProviderConf(const std::string& path, ProviderConf& out);

// Load all .conf provider profiles from bundled + user directories.
// Searches: ./data/providers/, /boot/system/data/pluto/providers/,
// then ~/config/settings/pluto/providers/.
std::vector<ProviderConf> LoadProviderConfs();

// Authentication credentials — either API key or OAuth token.
struct Credentials {
	enum Type { API_KEY, OAUTH } type = API_KEY;
	std::string token;  // API key string or OAuth access token
	bool empty() const { return token.empty(); }
};

// Resolve credentials: OAuth tokens first (if present and valid),
// then fall back to API key. Returns empty credentials on failure.
Credentials ResolveCredentials(Config& cfg);

// Returns true if no config file exists (first launch).
bool IsFirstRun();

// Interactive first-run wizard: asks the user to pick a provider and model.
// Writes the config file and updates cfg in place. Returns false if the
// user aborted (Ctrl+C or invalid input).
bool RunFirstRunWizard(Config& cfg);

// Load from ~/config/settings/pluto. Missing file is not an error; defaults
// are returned. Values from the file override defaults; env vars override
// both via ResolveApiKey below.
Config LoadConfig();

// Write the config back to ~/config/settings/pluto. Returns true on success.
bool SaveConfig(const Config& cfg);

// Auto-detect provider, base_url, and api_key_env from the model name.
// E.g. model "gpt-image-1" → provider "openai-compat", base_url OpenAI.
// Call after LoadConfig() to fix inconsistent configs (e.g. provider=anthropic
// but model=gpt-4.1). Also called on model switch in the GUI.
void AutoDetectProvider(Config& cfg);

// Create the appropriate Provider for the given config. Resolves credentials
// (OAuth → API key) and instantiates AnthropicProvider or OpenAICompatProvider.
// Centralised factory — use this instead of inline provider construction.
std::unique_ptr<Provider> CreateProvider(const Config& cfg);

// Guess model capabilities from model name. Used when the user switches
// model and the profile doesn't specify explicit values.
int GuessMaxOutputTokens(const std::string& model);
int GuessContextWindow(const std::string& model);

// Resolve API key: env var named in cfg.api_key_env first, then fall back
// to ~/config/settings/pluto-api-key (single line, chmod 600).
std::string ResolveApiKey(const Config& cfg);

// Check only the per-provider key file (no env var, no legacy fallback).
// Used by the settings dialog to show key status per provider.
std::string ResolveProviderKey(const std::string& env_name);

// Persist an API key with mode 0600.
// The two-arg form saves to ~/config/settings/pluto/keys/<env_name>
// (per-provider). The one-arg form saves to the legacy pluto-api-key file.
bool SaveApiKey(const std::string& key);
bool SaveApiKey(const std::string& key, const std::string& env_name);

// Append project-memory files to cfg.system_prompt:
//   ~/config/settings/pluto/pluto.md   (user memory, always)
//   ./pluto.md                         (project memory, if cwd is a project)
// Each file is wrapped in a <user_memory> / <project_memory> tag so the
// model can tell them apart. Equivalent to Claude Code's CLAUDE.md.
void ApplyProjectMemory(Config& cfg);

// Scan ~/config/settings/pluto/skills/<name>/SKILL.md and process skills:
// - Skills without a `trigger:` frontmatter field are injected into
//   cfg.system_prompt immediately (always-on, backward compatible).
// - Skills with `trigger: keyword1, keyword2` are stored in
//   cfg.triggered_skills and listed in a <available_skills> catalog
//   in the system prompt so the user knows they exist.
void ApplySkills(Config& cfg);

// Check if a user message matches any triggered skill keywords.
// If so, inject the skill body into cfg.system_prompt and remove it
// from triggered_skills. Returns the names of newly activated skills.
std::vector<std::string> InjectTriggeredSkills(Config& cfg,
                                                  const std::string& user_message);

// If the BeAPI RAG index is loaded, append a hint to cfg.system_prompt
// informing the model about the beapi_lookup tool.
void ApplyRagHint(Config& cfg);

// Load mode definitions from ~/config/settings/pluto/modes/*.md and
// register the built-in "plan" mode. Populates cfg.modes.
void LoadModes(Config& cfg);

// Find a mode by name. Returns nullptr if not found.
const ModeEntry* FindMode(const Config& cfg, const std::string& name);

}  // namespace pluto
