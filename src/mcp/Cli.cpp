#include "Cli.h"

#include "Config.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace pluto {

namespace {

const char* scope_name(McpServerSpec::Scope s) {
	switch (s) {
		case McpServerSpec::LOCAL:   return "local";
		case McpServerSpec::PROJECT: return "project";
		case McpServerSpec::USER:    return "user";
	}
	return "?";
}

bool parse_scope(const std::string& s, McpServerSpec::Scope& out) {
	if (s == "local")   { out = McpServerSpec::LOCAL;   return true; }
	if (s == "project") { out = McpServerSpec::PROJECT; return true; }
	if (s == "user")    { out = McpServerSpec::USER;    return true; }
	return false;
}

int cmd_list() {
	auto specs = LoadMcpSpecs();
	if (specs.empty()) {
		std::printf("No MCP servers configured.\n");
		std::printf("  project: %s\n", ProjectMcpPath().c_str());
		std::printf("  user:    %s\n", UserMcpPath().c_str());
		std::printf("  local:   %s\n", LocalMcpPath().c_str());
		return 0;
	}
	for (const auto& s : specs) {
		std::printf("%-20s  [%-7s] %s", s.name.c_str(), scope_name(s.scope), s.type.c_str());
		if (!s.url.empty()) std::printf("  %s", s.url.c_str());
		if (!s.command.empty()) {
			std::printf("  %s", s.command.c_str());
			for (const auto& a : s.args) std::printf(" %s", a.c_str());
		}
		std::printf("\n");
	}
	return 0;
}

int cmd_get(const std::string& name) {
	auto specs = LoadMcpSpecs();
	for (const auto& s : specs) {
		if (s.name != name) continue;
		std::printf("name:   %s\n", s.name.c_str());
		std::printf("scope:  %s  (%s)\n", scope_name(s.scope), s.source_path.c_str());
		std::printf("type:   %s\n", s.type.c_str());
		if (!s.url.empty())     std::printf("url:    %s\n", s.url.c_str());
		if (!s.command.empty()) {
			std::printf("cmd:    %s", s.command.c_str());
			for (const auto& a : s.args) std::printf(" %s", a.c_str());
			std::printf("\n");
		}
		for (const auto& [k, v] : s.env)
			std::printf("env:    %s=%s\n", k.c_str(), v.c_str());
		for (const auto& [k, v] : s.headers)
			std::printf("header: %s: %s\n", k.c_str(), v.c_str());
		return 0;
	}
	std::fprintf(stderr, "no server named '%s'\n", name.c_str());
	return 1;
}

int cmd_remove(int argc, char** argv) {
	auto scope = McpServerSpec::LOCAL;
	std::string name;
	for (int i = 0; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--scope" && i + 1 < argc) {
			if (!parse_scope(argv[++i], scope)) {
				std::fprintf(stderr, "bad scope\n"); return 2;
			}
		} else if (name.empty()) {
			name = a;
		}
	}
	if (name.empty()) { std::fprintf(stderr, "remove: name required\n"); return 2; }
	std::string err;
	if (!RemoveMcpSpec(name, scope, err)) {
		std::fprintf(stderr, "remove failed: %s\n", err.c_str());
		return 1;
	}
	std::printf("removed '%s' from %s scope\n", name.c_str(), scope_name(scope));
	return 0;
}

int cmd_add(int argc, char** argv) {
	// Options go before the name (matching Claude Code's style).
	// pluto mcp add [--transport t] [--scope s] [--env K=V]... [--header H]... NAME [URL | -- CMD ARGS...]
	std::string type;
	auto scope = McpServerSpec::LOCAL;
	std::vector<std::pair<std::string, std::string>> envs;
	std::vector<std::string> headers;
	std::string name;
	std::string url;
	std::vector<std::string> cmd;
	bool after_ddash = false;

	for (int i = 0; i < argc; ++i) {
		std::string a = argv[i];
		if (after_ddash) { cmd.push_back(a); continue; }
		if (a == "--") { after_ddash = true; continue; }
		if (a == "--transport" && i + 1 < argc) { type = argv[++i]; continue; }
		if (a == "--scope" && i + 1 < argc) {
			if (!parse_scope(argv[++i], scope)) {
				std::fprintf(stderr, "bad scope\n"); return 2;
			}
			continue;
		}
		if (a == "--env" && i + 1 < argc) {
			std::string kv = argv[++i];
			size_t eq = kv.find('=');
			if (eq == std::string::npos) {
				std::fprintf(stderr, "bad --env (expected KEY=VALUE)\n");
				return 2;
			}
			envs.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
			continue;
		}
		if (a == "--header" && i + 1 < argc) { headers.push_back(argv[++i]); continue; }
		// Positional
		if (name.empty()) name = a;
		else if (url.empty()) url = a;
		else { std::fprintf(stderr, "unexpected arg: %s\n", a.c_str()); return 2; }
	}
	if (name.empty()) {
		std::fprintf(stderr,
			"usage: pluto mcp add [--transport http|sse|stdio] [--scope local|project|user]\n"
			"                     [--env K=V]... [--header H]... NAME [URL | -- CMD ARGS...]\n");
		return 2;
	}

	// Infer type if not set.
	if (type.empty()) {
		if (!cmd.empty()) type = "stdio";
		else if (!url.empty()) type = "http";
		else type = "stdio";
	}

	McpServerSpec spec;
	spec.name = name;
	spec.type = type;
	spec.scope = scope;

	if (type == "http" || type == "sse") {
		if (url.empty()) {
			std::fprintf(stderr, "url required for transport=%s\n", type.c_str());
			return 2;
		}
		spec.url = url;
	} else if (type == "stdio") {
		if (cmd.empty()) {
			std::fprintf(stderr,
				"stdio requires a command after --; e.g. pluto mcp add NAME -- npx -y @foo/mcp\n");
			return 2;
		}
		spec.command = cmd[0];
		for (size_t i = 1; i < cmd.size(); ++i) spec.args.push_back(cmd[i]);
	} else {
		std::fprintf(stderr, "unknown transport: %s\n", type.c_str());
		return 2;
	}

	for (const auto& [k, v] : envs) spec.env[k] = v;
	for (const auto& h : headers) {
		size_t c = h.find(':');
		if (c == std::string::npos) continue;
		std::string k = h.substr(0, c);
		std::string v = h.substr(c + 1);
		while (!v.empty() && v.front() == ' ') v.erase(0, 1);
		spec.headers[k] = v;
	}

	std::string err;
	if (!SaveMcpSpec(spec, scope, err)) {
		std::fprintf(stderr, "save failed: %s\n", err.c_str());
		return 1;
	}
	std::printf("added '%s' to %s scope (%s)\n",
		name.c_str(), scope_name(scope),
		scope == McpServerSpec::PROJECT ? ProjectMcpPath().c_str()
			: scope == McpServerSpec::USER ? UserMcpPath().c_str()
			: LocalMcpPath().c_str());
	return 0;
}

int cmd_reset() {
	ResetProjectApprovals();
	char cwd[4096];
	const char* c = getcwd(cwd, sizeof(cwd));
	std::printf("project approvals cleared for %s\n", c ? c : "?");
	return 0;
}

int cmd_discover() {
	std::printf("Scanning localhost for MCP servers…\n");
	auto found = DiscoverMcpServers();
	if (found.empty()) {
		std::printf("  (no new MCP servers found)\n");
		return 0;
	}
	std::printf("Found %zu server(s):\n", found.size());
	for (const auto& s : found) {
		std::printf("  %-16s  %s  (%s)\n",
			s.name.c_str(), s.url.c_str(), s.type.c_str());
	}
	std::printf("\nTo add one, run:\n");
	std::printf("  pluto mcp add <name> --url <url> --type sse --scope user\n");
	return 0;
}

void usage() {
	std::fprintf(stderr,
		"usage: pluto mcp <subcommand>\n"
		"  list                     List configured MCP servers across all scopes\n"
		"  get NAME                 Show details of one server\n"
		"  add [options] NAME ...   Add a server (see 'pluto mcp add --help')\n"
		"  remove [--scope S] NAME  Remove a server from the given scope (default local)\n"
		"  discover                 Scan localhost ports for MCP servers\n"
		"  reset-project-choices    Clear project-scope approvals for the cwd\n");
}

}  // namespace

int RunMcpCli(int argc, char** argv) {
	if (argc < 1) { usage(); return 2; }
	std::string sub = argv[0];
	if (sub == "-h" || sub == "--help") { usage(); return 0; }
	if (sub == "list")   return cmd_list();
	if (sub == "get") {
		if (argc < 2) { std::fprintf(stderr, "get: name required\n"); return 2; }
		return cmd_get(argv[1]);
	}
	if (sub == "add")    return cmd_add(argc - 1, argv + 1);
	if (sub == "remove") return cmd_remove(argc - 1, argv + 1);
	if (sub == "discover") return cmd_discover();
	if (sub == "reset-project-choices") return cmd_reset();
	std::fprintf(stderr, "unknown subcommand: %s\n", sub.c_str());
	usage();
	return 2;
}

}  // namespace pluto
