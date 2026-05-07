#pragma once

#include <string>
#include <vector>

namespace pluto {

struct Agent {
	std::string name;
	std::string path;           // absolute path to AGENT.md
	std::string description;
	std::string author;
	std::string version;
	std::vector<std::string> tools;      // required tools
	std::vector<std::string> skills;     // bundled skill names
	std::vector<std::string> routines;   // bundled routine names
	std::vector<std::string> commands;   // bundled command names
};

// Directory for installed agents.
std::string AgentsDir();

// Parse an AGENT.md file. Returns false on error.
bool ParseAgent(const std::string& path, Agent& out);

// List all installed agents.
std::vector<Agent> ListAgents();

// Preview an agent before installation.
// Reads the AGENT.md, fills `agent` with metadata, returns the full
// body text (prompt). Returns false if AGENT.md is missing/invalid.
bool PreviewAgent(const std::string& source_dir, Agent& agent,
                   std::string& body);

// Install an agent from a local directory.
// Copies AGENT.md + bundled files to agents_dir()/<name>/.
// Registers skills, routines, and commands.
// Returns false on error (message in `error`).
bool InstallAgent(const std::string& source_dir, std::string& error);

// Install an agent from a Git URL.
// Clones to a temp dir, then calls install_agent on the result.
bool InstallAgentFromUrl(const std::string& url, std::string& error);

// Remove an installed agent and its registered skills/routines/commands.
bool RemoveAgent(const std::string& name, std::string& error);

// Check if required tools are available.
// Returns list of missing tool names.
std::vector<std::string> CheckAgentDeps(const Agent& a,
                                           const std::vector<std::string>& available_tools);

// Update an installed agent from its original source (Git URL).
// If the installed version matches the remote, does nothing.
// Returns false on error (message in `error`).
bool UpdateAgent(const std::string& name, std::string& error);

// Update all installed agents that have a source URL.
// Returns number of agents updated (or -1 on fatal error).
int UpdateAllAgents();

// Read the source URL stored at install time. Empty if local install.
std::string AgentSourceUrl(const std::string& name);

// Search installed agents and example agents by query string.
// Matches against name, description, author, and tools (case-insensitive).
std::vector<Agent> SearchAgents(const std::string& query);

// Default agent registry: a Git repository with index.json + agent dirs.
// Agents can be installed by short name: `pluto agent install code-reviewer`
// resolves to the default registry and clones just that agent's directory.
extern const char* kDefaultRegistryUrl;

// Install an agent by short name from the default registry.
// Clones the registry (shallow), finds the agent directory, installs it.
// Returns false on error (message in `error`).
bool InstallAgentFromRegistry(const std::string& name, std::string& error);

// Run the `pluto agent` subcommand (install/remove/list/update/search).
// Note: `pluto agent run` is handled separately in main.cpp.
int RunAgentStoreCli(int argc, char** argv);

}  // namespace pluto
