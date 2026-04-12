#pragma once

#include <map>
#include <string>
#include <vector>

namespace pluto {

struct McpServerSpec {
	enum Scope { LOCAL, PROJECT, USER };

	std::string name;
	std::string type;                              // "stdio" | "http" | "sse"
	std::string url;                               // http/sse
	std::string command;                           // stdio
	std::vector<std::string> args;                 // stdio
	std::map<std::string, std::string> env;        // stdio
	std::map<std::string, std::string> headers;    // http/sse

	Scope scope = LOCAL;
	std::string source_path;
};

// Merge local > project > user. Shadowed entries are dropped (warning on stderr).
std::vector<McpServerSpec> load_mcp_specs();

bool save_mcp_spec(const McpServerSpec& spec, McpServerSpec::Scope scope, std::string& err);
bool remove_mcp_spec(const std::string& name, McpServerSpec::Scope scope, std::string& err);

bool is_project_approved(const std::string& server_name);
void set_project_approved(const std::string& server_name);
void reset_project_approvals();

std::string project_mcp_path();
std::string user_mcp_path();
std::string local_mcp_path();
std::string approvals_path();

}  // namespace pluto
