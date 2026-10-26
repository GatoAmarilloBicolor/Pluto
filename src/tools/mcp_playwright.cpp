// Playwright MCP tools — browser automation.
// Wraps the Playwright MCP server (stdio) as Pluto tools.
// Provides navigate, click, fill_form, screenshot, snapshot, etc.

#include "../mcp/Client.h"
#include "../mcp/builtin/mcp_tool_wrapper.h"
#include "Tool.h"

#include <memory>
#include <string>

namespace pluto {

void register_playwright_tools(ToolRegistry& registry,
                               std::shared_ptr<McpClient> client) {
	int count = ::pluto::mcp::register_mcp_client_tools(client, registry, "playwright__");
	(void)count;
}

}  // namespace pluto
