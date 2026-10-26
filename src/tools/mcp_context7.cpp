// Context7 MCP tools — up-to-date library documentation.
// Wraps the Context7 MCP server (stdio) as Pluto tools.

#include "../mcp/Client.h"
#include "../mcp/builtin/mcp_tool_wrapper.h"
#include "Tool.h"

#include <memory>
#include <string>

namespace pluto {

void register_context7_tools(ToolRegistry& registry,
                             std::shared_ptr<McpClient> client) {
	int count = ::pluto::mcp::register_mcp_client_tools(client, registry, "context7__");
	(void)count;
}

}  // namespace pluto
