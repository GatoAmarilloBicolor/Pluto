#pragma once

#include <string>
#include <vector>
#include <map>

namespace pluto {
namespace mcp {

// A built-in MCP server definition: how to launch it and what tools it exposes.
struct BuiltinMcpServer {
	std::string name;           // e.g. "serena", "context7"
	std::string command;        // executable to launch (stdio transport)
	std::vector<std::string> args;
	std::map<std::string, std::string> env;  // extra environment variables
	bool auto_start = true;     // start automatically on Pluto launch
	bool require_approval = false; // prompt user before starting
	std::string description;    // human-readable description
};

// Registry of all built-in MCP servers Pluto knows about.
// Returns the full list; Pluto will launch those with auto_start=true.
std::vector<BuiltinMcpServer> list_builtin_mcp_servers();

// Returns the server by name, or nullptr if not found.
const BuiltinMcpServer* find_builtin_mcp_server(const std::string& name);

// Convenience: launch command strings for each server.
std::string builtin_server_command(const BuiltinMcpServer& s);

}  // namespace mcp
}  // namespace pluto
