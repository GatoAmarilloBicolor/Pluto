// SQLite MCP tools — database queries.
// Wraps the SQLite MCP server (stdio) as Pluto tools.
// Provides create-table, insert-record, query, update-record, delete-record,
// drop-table, list-tables, describe-table.

#include "mcp/Client.h"
#include "mcp/builtin/mcp_tool_wrapper.h"
#include "../tools/Tool.h"
#include "../tools/ToolRegistry.h"

#include <memory>
#include <string>

namespace pluto {

void register_sqlite_tools(ToolRegistry& registry,
                           std::shared_ptr<McpClient> client) {
	int count = register_mcp_client_tools(client, registry, "sqlite__");
	(void)count;
}

}  // namespace pluto
