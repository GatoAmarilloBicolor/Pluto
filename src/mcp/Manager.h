#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "../tools/Tool.h"
#include "Client.h"
#include "Config.h"

namespace pluto {

// Owns the lifetime of all connected MCP clients and registers their tools
// in a shared ToolRegistry. Constructed once per process.
class McpManager {
public:
	using ApproveFn = std::function<bool(const McpServerSpec& spec)>;

	// Loads .mcp.json + user + local scopes, prompts `approve` for project
	// scope unapproved entries, opens transports, calls connect, registers
	// tools. Failures are logged to stderr and skipped.
	McpManager(ToolRegistry& tools, ApproveFn approve);

	std::vector<std::shared_ptr<McpClient>> clients() const;

	// Add a pre-connected client (used by auto-discovery).
	void add_client(std::shared_ptr<McpClient> client, ToolRegistry& tools);

	// Generate a system prompt block that describes all connected MCP
	// servers and their tools, so the model knows what is available.
	std::string system_prompt_block() const;

private:
	std::vector<std::shared_ptr<McpClient>> fClients;
};

}  // namespace pluto