#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "../mcp/Client.h"
#include "../tools/Tool.h"
#include "../tools/ToolRegistry.h"

namespace pluto {
namespace mcp {

// MCPToolWrapper wraps a single MCP tool from a connected McpClient as a
// Pluto Tool, so the agent can call it through the standard tool registry.
class McpToolWrapper : public Tool {
public:
	McpToolWrapper(std::shared_ptr<McpClient> client,
	               const McpToolInfo& info,
	               const std::string& prefix);

	std::string name() const override { return fName; }
	std::string description() const override { return fDescription; }
	std::string input_schema_json() const override { return fInputSchema; }
	std::string preview(const std::string& input_json) const override;
	ToolResult execute(const std::string& input_json) override;

private:
	std::shared_ptr<McpClient> fClient;
	std::string fName;
	std::string fDescription;
	std::string fInputSchema;
};

// Register all tools from an MCP client into the registry, with an optional
// prefix (e.g. "serena__"). Returns the number of tools registered.
int register_mcp_client_tools(std::shared_ptr<McpClient> client,
                              ToolRegistry& registry,
                              const std::string& prefix = "");

}  // namespace mcp
}  // namespace pluto
