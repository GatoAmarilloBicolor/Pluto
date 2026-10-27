#include "Config.h"
#include "../version.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unistd.h>
#include <utility>

#include "../filelock.h"
#include "../json.h"
#include "../util.h"
#include "../workspace.h"

namespace pluto {

namespace {

std::string settings_dir() {
	return HomeDir() + "/config/settings";
}

std::string cwd() {
	// Prefer WorkspaceRoot() over getcwd() so that GUI processes
	// launched from Deskbar (where cwd != workspace) still find
	// project-scoped .mcp.json and mcp-local.json entries.
	std::string ws = WorkspaceRoot();
	if (!ws.empty()) return ws;
	char buf[4096];
	return getcwd(buf, sizeof(buf)) ? std::string(buf) : std::string();
}

// ${VAR} or ${VAR:-default}
std::string expand_env(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
			size_t end = s.find('}', i + 2);
			if (end == std::string::npos) { out += s[i++]; continue; }
			std::string inner = s.substr(i + 2, end - i - 2);
			std::string var = inner;
			std::string def;
			size_t colon = inner.find(":-");
			if (colon != std::string::npos) {
				var = inner.substr(0, colon);
				def = inner.substr(colon + 2);
			}
			const char* val = std::getenv(var.c_str());
			out += (val && *val) ? std::string(val) : def;
			i = end + 1;
		} else {
			out += s[i++];
		}
	}
	return out;
}

json::Value load_json_file(const std::string& path) {
	std::ifstream f(path);
	if (!f) return json::Value(json::Object{});
	std::stringstream ss;
	ss << f.rdbuf();
	try { return json::Value::parse(ss.str()); }
	catch (const std::exception& e) {
		std::fprintf(stderr, "[mcp] %s: parse error: %s\n", path.c_str(), e.what());
		return json::Value(json::Object{});
	}
}

bool write_json_file(const std::string& path, const json::Value& v, std::string& err) {
	// Atomic write: temp file + rename to avoid corruption on crash.
	std::string tmp = path + ".tmp." + std::to_string(getpid());
	{
		std::ofstream f(tmp, std::ios::trunc);
		if (!f) { err = "cannot open " + tmp + " for writing"; return false; }
		f << v.serialize();
		if (!f.good()) {
			std::remove(tmp.c_str());
			err = "write failed";
			return false;
		}
	}
	if (::rename(tmp.c_str(), path.c_str()) != 0) {
		std::remove(tmp.c_str());
		err = "rename failed";
		return false;
	}
	return true;
}

McpServerSpec parse_spec(const std::string& name, const json::Value& cfg,
                          McpServerSpec::Scope scope, const std::string& source) {
	McpServerSpec s;
	s.name = name;
	s.scope = scope;
	s.source_path = source;
	s.type = cfg.str("type");
	if (s.type.empty()) {
		if (!cfg.str("command").empty()) s.type = "stdio";
		else if (!cfg.str("url").empty()) s.type = "http";
		else s.type = "stdio";
	}
	s.url = expand_env(cfg.str("url"));
	s.command = expand_env(cfg.str("command"));

	if (const json::Value* args = cfg.find("args"); args && args->is_arr()) {
		for (const auto& a : args->as_arr()) {
			if (a.is_str()) s.args.push_back(expand_env(a.as_str()));
		}
	}
	if (const json::Value* env = cfg.find("env"); env && env->is_obj()) {
		for (const auto& [k, v] : env->as_obj()) {
			if (v.is_str()) s.env[k] = expand_env(v.as_str());
		}
	}
	if (const json::Value* hdrs = cfg.find("headers"); hdrs && hdrs->is_obj()) {
		for (const auto& [k, v] : hdrs->as_obj()) {
			if (v.is_str()) s.headers[k] = expand_env(v.as_str());
		}
	}
	return s;
}

std::vector<McpServerSpec> load_flat_file(const std::string& path, McpServerSpec::Scope scope) {
	std::vector<McpServerSpec> out;
	std::ifstream probe(path);
	if (!probe) return out;
	probe.close();
	json::Value v = load_json_file(path);
	const json::Value* servers = v.find("mcpServers");
	if (!servers || !servers->is_obj()) return out;
	for (const auto& [name, cfg] : servers->as_obj()) {
		if (cfg.is_obj()) out.push_back(parse_spec(name, cfg, scope, path));
	}
	return out;
}

std::vector<McpServerSpec> load_local_file() {
	std::vector<McpServerSpec> out;
	std::string path = LocalMcpPath();
	std::ifstream probe(path);
	if (!probe) return out;
	probe.close();
	json::Value v = load_json_file(path);
	if (!v.is_obj()) return out;
	std::string c = cwd();
	const json::Value* proj = v.find(c);
	if (!proj || !proj->is_obj()) return out;
	const json::Value* servers = proj->find("mcpServers");
	if (!servers || !servers->is_obj()) return out;
	for (const auto& [name, cfg] : servers->as_obj()) {
		if (cfg.is_obj()) out.push_back(parse_spec(name, cfg, McpServerSpec::LOCAL, path));
	}
	return out;
}

json::Value spec_to_value(const McpServerSpec& spec) {
	using namespace json;
	Value v(Object{});
	if (!spec.type.empty()) v.set("type", spec.type);
	if (!spec.url.empty()) v.set("url", spec.url);
	if (!spec.command.empty()) v.set("command", spec.command);
	if (!spec.args.empty()) {
		Array arr;
		for (const auto& a : spec.args) arr.push_back(Value(a));
		v.set("args", Value(std::move(arr)));
	}
	if (!spec.env.empty()) {
		Value e(Object{});
		for (const auto& [k, val] : spec.env) e.set(k, val);
		v.set("env", e);
	}
	if (!spec.headers.empty()) {
		Value h(Object{});
		for (const auto& [k, val] : spec.headers) h.set(k, val);
		v.set("headers", h);
	}
	return v;
}

}  // namespace

std::string ProjectMcpPath() { return cwd() + "/.mcp.json"; }
std::string UserMcpPath()    { return settings_dir() + "/pluto/mcp.json"; }
std::string LocalMcpPath()   { return settings_dir() + "/pluto/mcp-local.json"; }
std::string ApprovalsPath()   { return settings_dir() + "/pluto/mcp-approvals.json"; }

std::vector<McpServerSpec> LoadMcpSpecs() {
	auto local   = load_local_file();
	auto project = load_flat_file(ProjectMcpPath(), McpServerSpec::PROJECT);
	auto user    = load_flat_file(UserMcpPath(),    McpServerSpec::USER);

	std::vector<McpServerSpec> out;
	std::set<std::string> seen;
	auto take = [&](std::vector<McpServerSpec>& specs, const char* label) {
		for (auto& s : specs) {
			if (seen.count(s.name)) {
				std::fprintf(stderr,
					"[mcp] %s: '%s' shadowed by higher-precedence scope\n",
					label, s.name.c_str());
				continue;
			}
			seen.insert(s.name);
			out.push_back(std::move(s));
		}
	};
	take(local,   "local");
	take(project, "project");
	take(user,    "user");
	// Built-in System One (Laya-based decision model) - stdio transport
	if (seen.insert("systemone").second) {
		McpServerSpec sys;
		sys.name = "systemone";
		sys.scope = McpServerSpec::USER;
		sys.source_path = "<built-in>";
		sys.type = "stdio";
		sys.command = "systemone_server";
		out.push_back(std::move(sys));
	}
	return out;
}

bool SaveMcpSpec(const McpServerSpec& spec, McpServerSpec::Scope scope, std::string& err) {
	std::string path;
	switch (scope) {
		case McpServerSpec::PROJECT: path = ProjectMcpPath(); break;
		case McpServerSpec::USER:    path = UserMcpPath();    break;
		case McpServerSpec::LOCAL:   path = LocalMcpPath();   break;
	}

	json::Value root = load_json_file(path);
	if (!root.is_obj()) root = json::Value(json::Object{});

	json::Value new_entry = spec_to_value(spec);

	if (scope == McpServerSpec::LOCAL) {
		std::string c = cwd();
		if (c.empty()) { err = "getcwd failed"; return false; }
		const json::Value* existing_proj = root.find(c);
		json::Value proj = existing_proj ? *existing_proj : json::Value(json::Object{});
		if (!proj.is_obj()) proj = json::Value(json::Object{});
		const json::Value* existing_servers = proj.find("mcpServers");
		json::Value servers = existing_servers ? *existing_servers : json::Value(json::Object{});
		if (!servers.is_obj()) servers = json::Value(json::Object{});
		servers.set(spec.name, new_entry);
		proj.set("mcpServers", servers);
		root.set(c, proj);
	} else {
		const json::Value* existing_servers = root.find("mcpServers");
		json::Value servers = existing_servers ? *existing_servers : json::Value(json::Object{});
		if (!servers.is_obj()) servers = json::Value(json::Object{});
		servers.set(spec.name, new_entry);
		root.set("mcpServers", servers);
	}
	return write_json_file(path, root, err);
}

bool RemoveMcpSpec(const std::string& name, McpServerSpec::Scope scope, std::string& err) {
	std::string path;
	switch (scope) {
		case McpServerSpec::PROJECT: path = ProjectMcpPath(); break;
		case McpServerSpec::USER:    path = UserMcpPath();    break;
		case McpServerSpec::LOCAL:   path = LocalMcpPath();   break;
	}
	std::ifstream probe(path);
	if (!probe) { err = "no config at " + path; return false; }
	probe.close();

	json::Value root = load_json_file(path);
	if (!root.is_obj()) { err = "bad config"; return false; }

	if (scope == McpServerSpec::LOCAL) {
		std::string c = cwd();
		const json::Value* proj = root.find(c);
		if (!proj || !proj->is_obj()) { err = "no local entries for this project"; return false; }
		json::Value pmod = *proj;
		const json::Value* servers = pmod.find("mcpServers");
		if (!servers || !servers->is_obj()) { err = "not found"; return false; }
		json::Value smod = *servers;
		auto& obj = smod.as_obj();
		auto it = obj.find(name);
		if (it == obj.end()) { err = "not found"; return false; }
		obj.erase(it);
		pmod.set("mcpServers", smod);
		root.set(c, pmod);
	} else {
		const json::Value* servers = root.find("mcpServers");
		if (!servers || !servers->is_obj()) { err = "not found"; return false; }
		json::Value smod = *servers;
		auto& obj = smod.as_obj();
		auto it = obj.find(name);
		if (it == obj.end()) { err = "not found"; return false; }
		obj.erase(it);
		root.set("mcpServers", smod);
	}
	return write_json_file(path, root, err);
}

bool IsProjectApproved(const std::string& server_name) {
	json::Value v = load_json_file(ApprovalsPath());
	if (!v.is_obj()) return false;
	const json::Value* list = v.find(cwd());
	if (!list || !list->is_arr()) return false;
	for (const auto& e : list->as_arr()) {
		if (e.is_str() && e.as_str() == server_name) return true;
	}
	return false;
}

void SetProjectApproved(const std::string& server_name) {
	std::string lock_path = ApprovalsPath() + ".lock";
	ScopedFileLock flock(lock_path);
	json::Value v = load_json_file(ApprovalsPath());
	if (!v.is_obj()) v = json::Value(json::Object{});
	std::string c = cwd();
	const json::Value* existing = v.find(c);
	json::Value list = (existing && existing->is_arr()) ? *existing : json::Value(json::Array{});
	for (const auto& e : list.as_arr()) {
		if (e.is_str() && e.as_str() == server_name) return;
	}
	list.push(json::Value(server_name));
	v.set(c, list);
	std::string err;
	write_json_file(ApprovalsPath(), v, err);
}

void ResetProjectApprovals() {
	std::string lock_path = ApprovalsPath() + ".lock";
	ScopedFileLock flock(lock_path);
	json::Value v = load_json_file(ApprovalsPath());
	if (!v.is_obj()) return;
	auto& obj = v.as_obj();
	obj.erase(cwd());
	std::string err;
	write_json_file(ApprovalsPath(), v, err);
}

// ----------------------------------------------------------------
// Auto-discovery: probe well-known localhost ports for MCP servers.
// A HEAD request with a 1.5s timeout is sent to each endpoint. Any
// 200-level response counts as "alive". The list of probes is
// intentionally short — we're checking for known Haiku-ecosystem
// servers and common development ports.
// ----------------------------------------------------------------

// Extract "host:port" from a URL for dedup purposes.
std::string url_host_port(const std::string& url) {
	// Skip scheme ("http://", "https://")
	size_t s = url.find("://");
	if (s == std::string::npos) s = 0; else s += 3;
	size_t e = url.find('/', s);
	return url.substr(s, e == std::string::npos ? std::string::npos : e - s);
}

std::vector<DiscoveredServer> DiscoverMcpServers() {
	struct Probe {
		const char* name;
		const char* url;
		const char* type;
	};
	// Probe both /sse and /mcp paths per port — servers may expose either.
	static const Probe kProbes[] = {
		{ "pippo",    "http://localhost:2607/sse",  "sse"  },
		{ "pippo",    "http://localhost:2607/mcp",  "http" },
		{ "mcp-3000", "http://localhost:3000/sse",  "sse"  },
		{ "mcp-8080", "http://localhost:8080/sse",  "sse"  },
		{ "mcp-8000", "http://localhost:8000/sse",  "sse"  },
		{ "mcp-4000", "http://localhost:4000/sse",  "sse"  },
	};

	// Build a set of already-configured host:port pairs so we skip servers
	// that are already in .mcp.json / mcp.json / mcp-local.json regardless
	// of which path they're configured at.
	std::set<std::string> known_hosts;
	for (const auto& s : LoadMcpSpecs()) {
		if (!s.url.empty())
			known_hosts.insert(url_host_port(s.url));
	}

	std::vector<DiscoveredServer> out;
	std::set<std::string> found_hosts;  // avoid duplicate results per port

	// Reuse a single CURL handle across all probes.
	CURL* curl = curl_easy_init();
	if (!curl) return out;

	for (const auto& p : kProbes) {
		std::string hp = url_host_port(p.url);
		if (known_hosts.count(hp)) continue;    // already configured
		if (found_hosts.count(hp)) continue;     // already found via other path

		curl_easy_setopt(curl, CURLOPT_URL, p.url);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUTO_USER_AGENT);
		CURLcode rc = curl_easy_perform(curl);
		long code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		curl_easy_reset(curl);  // reset for next probe, keep connection pool

		if (rc == CURLE_OK && code >= 200 && code < 300) {
			std::fprintf(stderr, "[discover] found %s at %s\n",
				p.name, p.url);
			out.push_back(DiscoveredServer{p.name, p.url, p.type});
			found_hosts.insert(hp);
		}
	}
	curl_easy_cleanup(curl);
	return out;
}

}  // namespace pluto
