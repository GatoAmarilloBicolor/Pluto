#include "Manager.h"

#include <cstdio>

#include "Transport.h"

namespace pluto {

McpManager::McpManager(ToolRegistry& tools, ApproveFn approve) {
	for (auto& spec : LoadMcpSpecs()) {
		if (spec.scope == McpServerSpec::PROJECT
		    && !IsProjectApproved(spec.name)) {
			if (!approve || !approve(spec)) {
				std::fprintf(stderr, "[mcp/%s] denied\n", spec.name.c_str());
				continue;
			}
		}

		std::string err;
		auto transport = MakeTransport(spec, err);
		if (!transport) {
			std::fprintf(stderr, "[mcp/%s] transport: %s\n",
				spec.name.c_str(), err.c_str());
			continue;
		}
		if (!transport->start(err)) {
			std::fprintf(stderr, "[mcp/%s] start: %s\n",
				spec.name.c_str(), err.c_str());
			continue;
		}

		auto client = std::make_shared<McpClient>(spec.name, std::move(transport));
		if (!client->connect()) {
			std::fprintf(stderr, "[mcp/%s] connect failed\n",
				spec.name.c_str());
			continue;
		}
		std::fprintf(stderr, "[mcp/%s] %zu tools\n",
			spec.name.c_str(), client->tools().size());

		register_mcp_tools(client, tools);
		fClients.push_back(std::move(client));
	}
}

void McpManager::add_client(std::shared_ptr<McpClient> client,
                             ToolRegistry& tools) {
	register_mcp_tools(client, tools);
	fClients.push_back(std::move(client));
}

std::vector<std::shared_ptr<McpClient>> McpManager::clients() const {
	std::vector<std::shared_ptr<McpClient>> out;
	out.reserve(fClients.size());
	for (const auto& c : fClients) out.push_back(c);
	return out;
}

std::string McpManager::system_prompt_block() const {
	if (fClients.empty()) return {};

	std::string out;
	out += "<mcp_servers>\n";
	out += "The following MCP servers are connected. Their tools are available ";
	out += "for you to call. Prefer MCP tools when the task matches their ";
	out += "capabilities.\n";

	for (const auto& client : fClients) {
		out += "\n<server name=\"";
		out += client->server_name();
		out += "\">\n";
		out += "Tools:\n";
		for (const auto& tool : client->tools()) {
			// Skip per-app wrappers (same filter as register_mcp_tools).
			if (tool.name.size() >= 4 && tool.name.substr(0, 4) == "app_")
				continue;
			out += "- ";
			out += client->server_name();
			out += "__";
			out += tool.name;
			if (!tool.description.empty()) {
				out += ": ";
				out += tool.description;
			}
			out += "\n";
		}
		out += "</server>\n";
	}

	out += "</mcp_servers>";
	return out;
}

}  // namespace pluto