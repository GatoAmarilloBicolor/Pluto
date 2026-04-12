#include "config.h"

#include <cstdlib>
#include <fstream>
#include <string>

namespace pluto {

namespace {

std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
	return s.substr(a, b - a);
}

std::string config_path() {
	const char* home = std::getenv("HOME");
	if (!home) home = "/boot/home";
	return std::string(home) + "/config/settings/pluto";
}

}  // namespace

Config load_config() {
	Config cfg;
	std::ifstream f(config_path());
	if (!f) return cfg;
	std::string line;
	while (std::getline(f, line)) {
		std::string s = trim(line);
		if (s.empty() || s[0] == '#') continue;
		size_t eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(s.substr(0, eq));
		std::string v = trim(s.substr(eq + 1));
		if      (k == "provider")     cfg.provider = v;
		else if (k == "model")        cfg.model = v;
		else if (k == "base_url")     cfg.base_url = v;
		else if (k == "api_key_env")  cfg.api_key_env = v;
		else if (k == "system")       cfg.system_prompt = v;
		else if (k == "max_tokens")   cfg.max_tokens = std::atoi(v.c_str());
	}
	return cfg;
}

std::string resolve_api_key(const Config& cfg) {
	const char* v = std::getenv(cfg.api_key_env.c_str());
	return v ? std::string(v) : std::string();
}

}  // namespace pluto
