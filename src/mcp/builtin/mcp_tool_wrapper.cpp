#include "mcp_tool_wrapper.h"

#include <cstdio>

namespace pluto {
namespace mcp {

McpToolWrapper::McpToolWrapper(std::shared_ptr<McpClient> client,
                                const McpToolInfo& info,
                                const std::string& prefix)
	: fClient(std::move(client)),
	  fName(prefix + info.name),
	  fDescription(info.description),
	  fInputSchema(info.input_schema_json) {}

std::string McpToolWrapper::preview(const std::string& input_json) const {
	return fName + " " + input_json.substr(0, 100);
}

ToolResult McpToolWrapper::execute(const std::string& input_json) {
	ToolResult r;
	if (!fClient) {
		r.is_error = true;
		r.output = "MCP client not available";
		return r;
	}
	// The underlying tool name is without prefix; strip it for the call.
	std::string remote_name = fName;
	if (!fClient->server_name().empty() &&
	    remote_name.find(fClient->server_name() + "__") == 0) {
		remote_name = remote_name.substr(fClient->server_name().size() + 2);
	}
	try {
		r = fClient->call_tool(remote_name, input_json);
	} catch (const std::exception& e) {
		r.is_error = true;
		r.output = e.what();
	}
	return r;
}

int register_mcp_client_tools(std::shared_ptr<McpClient> client,
                               ToolRegistry& registry,
                               const std::string& prefix) {
	int count = 0;
	for (const auto& info : client->tools()) {
		registry.add(std::make_unique<McpToolWrapper>(client, info, prefix));
		count++;
	}
	return count;
}

}  // namespace mcp
}  // namespace pluto
