#pragma once

#include "../tools/Tool.h"
#include "Transport.h"

#include <memory>
#include <string>
#include <vector>

namespace pluto {

struct McpToolInfo {
	std::string name;
	std::string description;
	std::string input_schema_json;
};

// JSON-RPC 2.0 client for MCP 2024-11-05. Delegates transport to a Transport
// implementation (http, stdio, sse).
class McpClient {
public:
	McpClient(std::string server_name, std::unique_ptr<Transport> transport);
	~McpClient();

	// initialize + notifications/initialized + tools/list.
	bool connect();

	const std::vector<McpToolInfo>& tools() const { return fTools; }
	const std::string& server_name() const { return fServerName; }

	ToolResult call_tool(const std::string& name, const std::string& arguments_json);

private:
	std::string fServerName;
	std::unique_ptr<Transport> fTransport;
	int fNextId = 1;
	std::vector<McpToolInfo> fTools;
};

// Register every tool from client into registry, prefixed "<server>__<tool>".
// The client must outlive the registry.
void register_mcp_tools(McpClient& client, ToolRegistry& registry);

}  // namespace pluto
