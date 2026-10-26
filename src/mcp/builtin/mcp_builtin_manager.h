#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "../Client.h"
#include "../Transport.h"
#include "../../tools/Tool.h"
#include "builtin_servers.h"

namespace pluto {
namespace mcp {

// BuiltinMcpManager launches built-in MCP servers (stdio transport),
// connects to them, and registers their tools into a ToolRegistry.
// Each server gets a tool prefix (e.g. "serena__", "context7__").
class BuiltinMcpManager {
public:
	BuiltinMcpManager();
	~BuiltinMcpManager();

	// Initialize all auto-start built-in servers.
	// Returns the number of servers successfully connected.
	int init(ToolRegistry& registry);

	// Get a connected client by server name (or nullptr if not connected).
	std::shared_ptr<McpClient> client(const std::string& name) const;

	// Call a tool on a specific server: "server_name__tool_name" or just
	// "tool_name" if unambiguous.
	ToolResult call_tool(const std::string& tool_name,
	                     const std::string& arguments_json);

	// Generate a system prompt block describing all connected servers.
	std::string system_prompt_block() const;

	// Number of connected servers.
	int connected_count() const { return static_cast<int>(fClients.size()); }

private:
	std::map<std::string, std::shared_ptr<McpClient>> fClients;
	std::map<std::string, BuiltinMcpServer> fServerConfigs;

	// Launch a single server and register its tools.
	int launch_server(const BuiltinMcpServer& spec, ToolRegistry& registry);

	// Transport factory: create a stdio transport for the server command.
	std::unique_ptr<Transport> make_stdio_transport(
		const BuiltinMcpServer& s,
		std::string& err);
};

}  // namespace mcp
}  // namespace pluto
