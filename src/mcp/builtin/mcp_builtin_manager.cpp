#include "mcp_builtin_manager.h"
#include "../mcp/StdioTransport.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace pluto {
namespace mcp {

BuiltinMcpManager::BuiltinMcpManager() {}

BuiltinMcpManager::~BuiltinMcpManager() {}

	std::unique_ptr<Transport>
	BuiltinMcpManager::make_stdio_transport(const BuiltinMcpServer& s,
                                             std::string& err) {
		try {
			return std::make_unique<StdioTransport>(s.command, s.args, s.env);
		} catch (const std::exception& e) {
			err = e.what();
			return nullptr;
		}
	}

int BuiltinMcpManager::launch_server(const BuiltinMcpServer& spec,
                                      ToolRegistry& registry) {
	std::string err;
	auto transport = make_stdio_transport(spec, err);
	if (!transport) {
		fprintf(stderr, "[mcp/builtin/%s] transport error: %s\n",
			spec.name.c_str(), err.c_str());
		return 0;
	}

	auto client = std::make_shared<McpClient>(spec.name, std::move(transport));
	if (!client->connect()) {
		fprintf(stderr, "[mcp/builtin/%s] connect failed\n", spec.name.c_str());
		return 0;
	}

	int count = register_mcp_client_tools(client, registry, spec.name + "__");
	fClients[spec.name] = client;
	fServerConfigs[spec.name] = spec;

	fprintf(stderr, "[mcp/builtin/%s] connected, %d tools\n",
		spec.name.c_str(), count);
	return count;
}

int BuiltinMcpManager::init(ToolRegistry& registry) {
	int total = 0;
	auto servers = list_builtin_mcp_servers();
	for (auto& s : servers) {
		if (s.auto_start) {
			total += launch_server(s, registry);
		}
	}
	return total;
}

std::shared_ptr<McpClient> BuiltinMcpManager::client(
	const std::string& name) const {
	auto it = fClients.find(name);
	if (it != fClients.end()) return it->second;
	return nullptr;
}

ToolResult BuiltinMcpManager::call_tool(const std::string& tool_name,
                                         const std::string& arguments_json) {
	ToolResult r;
	r.is_error = true;

	// Parse "server__tool" or just "tool".
	std::string server_name, tool_name;
	size_t pos = tool_name.find("__");
	if (pos != std::string::npos) {
		server_name = tool_name.substr(0, pos);
		tool_name = tool_name.substr(pos + 2);
	} else {
		// Try to find the tool in any connected server.
		for (auto& [name, client] : fClients) {
			for (const auto& info : client->tools()) {
				if (info.name == tool_name) {
					server_name = name;
					break;
				}
			}
			if (!server_name.empty()) break;
		}
		if (server_name.empty()) {
			r.output = "MCP tool not found: " + tool_name;
			return r;
		}
	}

	auto it = fClients.find(server_name);
	if (it == fClients.end()) {
		r.output = "MCP server not connected: " + server_name;
		return r;
	}

	try {
		r = it->second->call_tool(tool_name, arguments_json);
	} catch (const std::exception& e) {
		r.is_error = true;
		r.output = e.what();
	}
	return r;
}

std::string BuiltinMcpManager::system_prompt_block() const {
	if (fClients.empty()) return "";

	std::string out;
	out += "\n<mcp_servers>\n";
	out += "The following MCP servers are connected. Their tools are available "
	       "for you to call. Use the tool name format: <server>__<tool>.\n\n";

	for (const auto& [name, client] : fClients) {
		out += "Server: " + name + "\n";
		out += "Description: ";
		auto spec_it = fServerConfigs.find(name);
		if (spec_it != fServerConfigs.end()) {
			out += spec_it->second.description;
		}
		out += "\nTools:\n";
		for (const auto& tool : client->tools()) {
			out += "  - " + name + "__" + tool.name;
			if (!tool.description.empty()) {
				out += ": " + tool.description;
			}
			out += "\n";
		}
		out += "\n";
	}
	out += "</mcp_servers>\n";
	return out;
}

}  // namespace mcp
}  // namespace pluto
