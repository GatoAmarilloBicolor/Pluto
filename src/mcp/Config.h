#pragma once

#include <map>
#include <string>
#include <vector>

namespace pluto {

struct McpServerSpec {
	enum Scope { LOCAL, PROJECT, USER };

	std::string name;
	std::string type;                              // "stdio" | "http" | "sse"
	std::string url;                               // http/sse
	std::string command;                           // stdio
	std::vector<std::string> args;                 // stdio
	std::map<std::string, std::string> env;        // stdio
	std::map<std::string, std::string> headers;    // http/sse

	Scope scope = LOCAL;
	std::string source_path;
};

// Merge local > project > user. Shadowed entries are dropped (warning on stderr).
std::vector<McpServerSpec> LoadMcpSpecs();

bool SaveMcpSpec(const McpServerSpec& spec, McpServerSpec::Scope scope, std::string& err);
bool RemoveMcpSpec(const std::string& name, McpServerSpec::Scope scope, std::string& err);

bool IsProjectApproved(const std::string& server_name);
void SetProjectApproved(const std::string& server_name);
void ResetProjectApprovals();

std::string ProjectMcpPath();
std::string UserMcpPath();
std::string LocalMcpPath();
std::string ApprovalsPath();

// Auto-discovery: probe well-known localhost ports for MCP servers.
// Returns specs for servers that responded. Quick (1-2s timeout per probe).
struct DiscoveredServer {
	std::string name;
	std::string url;
	std::string type;   // "sse" | "http"
};
std::vector<DiscoveredServer> DiscoverMcpServers();

}  // namespace pluto
