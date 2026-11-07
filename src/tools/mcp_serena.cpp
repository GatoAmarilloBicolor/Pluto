// Serena MCP tools — codebase exploration (symbol search, file read/edit, LSP).
// Wraps the Serena MCP server (stdio) as Pluto tools.

#include "../mcp/Client.h"
#include "../mcp/builtin/mcp_tool_wrapper.h"
#include "Tool.h"
#include <memory>
#include <string>

namespace pluto {

// Register Serena tools into the registry.
void register_serena_tools(ToolRegistry& registry,
                           std::shared_ptr<McpClient> client) {
	int count = ::pluto::mcp::register_mcp_client_tools(client, registry, "serena__");
	(void)count;
}

}  // namespace pluto
