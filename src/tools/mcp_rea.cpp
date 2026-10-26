// REA MCP tools — Reasoning/decision model server.
// Wraps the REA MCP server (stdio) as Pluto tools.
// Provides classification, verification, triage, guardrails, and more.

#include "../mcp/Client.h"
#include "../mcp/builtin/mcp_tool_wrapper.h"
#include "Tool.h"

#include <memory>
#include <string>

namespace pluto {

void register_rea_tools(ToolRegistry& registry,
                        std::shared_ptr<McpClient> client) {
	int count = ::pluto::mcp::register_mcp_client_tools(client, registry, "rea__");
	(void)count;
}

}  // namespace pluto
