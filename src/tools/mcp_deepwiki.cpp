// DeepWiki MCP tools — AI-powered GitHub repository documentation.
// Wraps the DeepWiki MCP server (stdio) as Pluto tools.

#include "mcp/Client.h"
#include "mcp/builtin/mcp_tool_wrapper.h"
#include "../tools/Tool.h"
#include "../tools/ToolRegistry.h"

#include <memory>
#include <string>

namespace pluto {

void register_deepwiki_tools(ToolRegistry& registry,
                             std::shared_ptr<McpClient> client) {
	int count = register_mcp_client_tools(client, registry, "deepwiki__");
	(void)count;
}

}  // namespace pluto
