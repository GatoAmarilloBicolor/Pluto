#include "builtin_servers.h"

#include <cstdlib>
#include <cstring>

namespace pluto {
namespace mcp {

std::vector<BuiltinMcpServer> list_builtin_mcp_servers() {
	std::vector<BuiltinMcpServer> servers;

	// --- Serena (Serena codebase exploration) ---
	// Uses the Serena MCP server via stdio.
	{
		BuiltinMcpServer s;
		s.name = "serena";
		s.command = "serena-mcp-server";  // or the path to the serena MCP binary
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "Serena codebase exploration: symbol search, file read/edit, LSP operations.";
		servers.push_back(std::move(s));
	}

	// --- Context7 (library documentation) ---
	{
		BuiltinMcpServer s;
		s.name = "context7";
		s.command = "context7-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "Up-to-date library documentation and code examples for any programming library.";
		servers.push_back(std::move(s));
	}

	// --- DeepWiki (GitHub repository documentation) ---
	{
		BuiltinMcpServer s;
		s.name = "deepwiki";
		s.command = "deepwiki-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "AI-powered documentation for GitHub repositories: wiki contents, structure, Q&A.";
		servers.push_back(std::move(s));
	}

	// --- REA (Reasoning/Decision model server) ---
	{
		BuiltinMcpServer s;
		s.name = "rea";
		s.command = "rea-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "REA reasoning and decision model: classification, verification, triage, guardrails.";
		servers.push_back(std::move(s));
	}

	// --- Memory (persistent knowledge graph) ---
	{
		BuiltinMcpServer s;
		s.name = "memory";
		s.command = "memory-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "Persistent knowledge graph: create entities, relations, observations, search nodes.";
		servers.push_back(std::move(s));
	}

	// --- Playwright (browser automation) ---
	{
		BuiltinMcpServer s;
		s.name = "playwright";
		s.command = "playwright-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "Browser automation: navigate, click, fill forms, take screenshots, extract content.";
		servers.push_back(std::move(s));
	}

	// --- SQLite (database queries) ---
	{
		BuiltinMcpServer s;
		s.name = "sqlite";
		s.command = "sqlite-mcp-server";
		s.args = {};
		s.auto_start = true;
		s.require_approval = false;
		s.description = "SQLite database: create tables, insert, query, update, delete records.";
		servers.push_back(std::move(s));
	}

	return servers;
}

const BuiltinMcpServer* find_builtin_mcp_server(const std::string& name) {
	auto servers = list_builtin_mcp_servers();
	for (auto& s : servers) {
		if (s.name == name) return &s;
	}
	return nullptr;
}

std::string builtin_server_command(const BuiltinMcpServer& s) {
	std::string cmd = s.command;
	for (const auto& a : s.args) {
		cmd += " " + a;
	}
	return cmd;
}

}  // namespace mcp
}  // namespace pluto
