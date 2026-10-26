// Memory MCP tools — persistent knowledge graph.
// Wraps the Memory MCP server (stdio) as Pluto tools.
// Provides entity/relation management, search, and observations.

#include "mcp/Client.h"
#include "mcp/builtin/mcp_tool_wrapper.h"
#include "../tools/Tool.h"
#include "../tools/ToolRegistry.h"

#include <memory>
#include <string>

namespace pluto {

void register_memory_tools(ToolRegistry& registry,
                           std::shared_ptr<McpClient> client) {
	int count = register_mcp_client_tools(client, registry, "memory__");
	(void)count;
}

}  // namespace pluto
