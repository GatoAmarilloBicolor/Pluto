#include "Config.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unistd.h>
#include <utility>

#include "../json.h"

namespace pluto {

namespace {

std::string home_dir() {
	const char* h = std::getenv("HOME");
	return h ? std::string(h) : std::string("/boot/home");
}

std::string settings_dir() {
	return home_dir() + "/config/settings";
}

std::string cwd() {
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
	std::ofstream f(path);
	if (!f) { err = "cannot open " + path + " for writing"; return false; }
	f << v.serialize();
	if (!f) { err = "write failed"; return false; }
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
	std::string path = local_mcp_path();
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

std::string project_mcp_path() { return cwd() + "/.mcp.json"; }
std::string user_mcp_path()    { return settings_dir() + "/pluto/mcp.json"; }
std::string local_mcp_path()   { return settings_dir() + "/pluto/mcp-local.json"; }
std::string approvals_path()   { return settings_dir() + "/pluto/mcp-approvals.json"; }

std::vector<McpServerSpec> load_mcp_specs() {
	auto local   = load_local_file();
	auto project = load_flat_file(project_mcp_path(), McpServerSpec::PROJECT);
	auto user    = load_flat_file(user_mcp_path(),    McpServerSpec::USER);

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
	return out;
}

bool save_mcp_spec(const McpServerSpec& spec, McpServerSpec::Scope scope, std::string& err) {
	std::string path;
	switch (scope) {
		case McpServerSpec::PROJECT: path = project_mcp_path(); break;
		case McpServerSpec::USER:    path = user_mcp_path();    break;
		case McpServerSpec::LOCAL:   path = local_mcp_path();   break;
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

bool remove_mcp_spec(const std::string& name, McpServerSpec::Scope scope, std::string& err) {
	std::string path;
	switch (scope) {
		case McpServerSpec::PROJECT: path = project_mcp_path(); break;
		case McpServerSpec::USER:    path = user_mcp_path();    break;
		case McpServerSpec::LOCAL:   path = local_mcp_path();   break;
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

bool is_project_approved(const std::string& server_name) {
	json::Value v = load_json_file(approvals_path());
	if (!v.is_obj()) return false;
	const json::Value* list = v.find(cwd());
	if (!list || !list->is_arr()) return false;
	for (const auto& e : list->as_arr()) {
		if (e.is_str() && e.as_str() == server_name) return true;
	}
	return false;
}

void set_project_approved(const std::string& server_name) {
	json::Value v = load_json_file(approvals_path());
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
	write_json_file(approvals_path(), v, err);
}

void reset_project_approvals() {
	json::Value v = load_json_file(approvals_path());
	if (!v.is_obj()) return;
	auto& obj = v.as_obj();
	obj.erase(cwd());
	std::string err;
	write_json_file(approvals_path(), v, err);
}

}  // namespace pluto
