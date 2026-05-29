#include "config.h"
#include "filelock.h"
#include "frontmatter.h"
#include "oauth.h"
#include "provider/Anthropic.h"
#include "provider/OpenAICompat.h"
#include "rag.h"
#include "util.h"

#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>
#include <image.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace pluto {

namespace {

std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
	return s.substr(a, b - a);
}

std::string home_dir() {
	BPath path;
	if (find_directory(B_USER_DIRECTORY, &path) == B_OK)
		return path.Path();
	return HomeDir();
}

std::string settings_dir() {
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
		path.Append("pluto");
		return path.Path();
	}
	return home_dir() + "/config/settings/pluto";
}

std::string config_path() {
	// Config file lives inside the pluto settings directory so that
	// sub-directories (sessions/, skills/, commands/) can coexist.
	// Legacy: if ~/config/settings/pluto is a FILE (old layout), read
	// from it directly for backward compat; writes go to the new path.
	std::string dir = settings_dir();
	struct stat st;
	if (::stat(dir.c_str(), &st) == 0 && S_ISREG(st.st_mode))
		return dir;  // legacy: pluto IS the config file
	return dir + "/config";
}

std::string api_key_path() {
	return home_dir() + "/config/settings/pluto-api-key";
}

// Migrate from legacy layout where ~/config/settings/pluto was a flat file
// to the new layout where it's a directory containing a `config` file.
// This only runs once; after migration the directory exists and sub-dirs
// (sessions, skills, commands, mcp-approvals.json) can be created inside it.
void maybe_migrate_settings() {
	std::string path = settings_dir();
	struct stat st;
	if (::stat(path.c_str(), &st) != 0) {
		// Doesn't exist yet — create the directory.
		::mkdir(path.c_str(), 0755);
		return;
	}
	if (S_ISDIR(st.st_mode)) return;  // already a directory, nothing to do.

	// It's a file (legacy). Rename it, create the directory, move it inside.
	std::string tmp = path + ".migrating";
	if (::rename(path.c_str(), tmp.c_str()) != 0) return;
	if (::mkdir(path.c_str(), 0755) != 0) {
		// mkdir failed — roll back to avoid losing the config.
		::rename(tmp.c_str(), path.c_str());
		return;
	}
	std::string dest = path + "/config";
	if (::rename(tmp.c_str(), dest.c_str()) != 0) {
		// Move failed — roll back: remove dir, restore file.
		::rmdir(path.c_str());
		::rename(tmp.c_str(), path.c_str());
		return;
	}
	std::fprintf(stderr, "[pluto] migrated config: %s → %s\n",
		path.c_str(), dest.c_str());
}

}  // namespace

bool IsFirstRun() {
	struct stat st;
	std::string path = config_path();
	return ::stat(path.c_str(), &st) != 0;
}

// ----------------------------------------------------------------
// .conf provider profile loading (shared by CLI wizard and GUI)
// ----------------------------------------------------------------

bool ParseProviderConf(const std::string& path, ProviderConf& out) {
	std::ifstream f(path);
	if (!f) return false;
	std::string line;
	while (std::getline(f, line)) {
		std::string s = trim(line);
		if (s.empty() || s[0] == '#') continue;
		size_t eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(s.substr(0, eq));
		std::string v = trim(s.substr(eq + 1));
		if (k == "label")            out.label = v;
		else if (k == "base_url")    out.base_url = v;
		else if (k == "api_key_env") out.api_key_env = v;
		else if (k == "model") {
			ProviderConfModel m;
			std::istringstream ss(v);
			std::string tok;
			int field = 0;
			while (std::getline(ss, tok, ',')) {
				tok = trim(tok);
				switch (field++) {
					case 0: m.id = tok; break;
					case 1: m.label = tok; break;
					case 2: m.max_tokens = std::atoi(tok.c_str()); break;
					case 3: m.context_window = std::atoi(tok.c_str()); break;
				}
			}
			if (m.label.empty()) m.label = m.id;
			if (!m.id.empty()) out.models.push_back(std::move(m));
		}
	}
	return !out.label.empty();
}

namespace {

void load_confs_from(const std::string& dir, std::vector<ProviderConf>& out) {
	BDirectory bdir(dir.c_str());
	if (bdir.InitCheck() != B_OK) return;
	std::vector<std::string> files;
	BEntry entry;
	while (bdir.GetNextEntry(&entry) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		if (entry.GetName(name) != B_OK) continue;
		std::string s = name;
		if (s.size() < 6 || s.substr(s.size() - 5) != ".conf") continue;
		files.push_back(s);
	}
	std::sort(files.begin(), files.end());
	for (const auto& name : files) {
		ProviderConf p;
		if (ParseProviderConf(dir + "/" + name, p))
			out.push_back(std::move(p));
	}
}

}  // namespace

std::vector<ProviderConf> LoadProviderConfs() {
	std::vector<ProviderConf> out;
	// 1. Built-in providers shipped with the package.
	//    Try relative path first (works when run from source tree),
	//    then resolve relative to the executable (works when installed).
	load_confs_from("data/providers", out);
	if (out.empty()) {
		// Resolve exe directory via Haiku's image_info.
		image_info info;
		int32 cookie = 0;
		if (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
			BPath exePath(info.name);
			exePath.GetParent(&exePath);
			std::string exeDir = exePath.Path();
			load_confs_from(exeDir + "/data/providers", out);
		}
	}
	if (out.empty())
		load_confs_from("/boot/system/data/pluto/providers", out);
	// 2. User-defined providers (override or extend).
	load_confs_from(SettingsDir() + "/providers", out);
	return out;
}

bool RunFirstRunWizard(Config& cfg) {
	std::printf("\n");
	std::printf("  \x1b[1mWelcome to Pluto!\x1b[0m\n\n");
	std::printf("  Choose your LLM provider:\n\n");

	// Anthropic is always option 1 (native provider, not from .conf).
	std::printf("    1. Anthropic (Claude)         — API key or Claude Max/Pro\n");

	// Load all .conf providers dynamically.
	auto confs = LoadProviderConfs();
	for (size_t i = 0; i < confs.size(); ++i) {
		const auto& p = confs[i];
		std::string hint;
		if (p.api_key_env.empty())
			hint = "no key needed";
		else
			hint = "API key (" + p.api_key_env + ")";
		std::printf("   %2zu. %-28s — %s\n", i + 2, p.label.c_str(), hint.c_str());
	}

	std::printf("\n  > ");
	std::fflush(stdout);

	char buf[32];
	if (!std::fgets(buf, sizeof(buf), stdin))
		return false;

	int choice = std::atoi(buf);

	if (choice == 1) {
		// Anthropic (native provider).
		cfg.provider = "anthropic";
		cfg.model = "claude-sonnet-4-5";
		cfg.api_key_env = "ANTHROPIC_API_KEY";
		cfg.base_url.clear();
		std::printf("\n  Provider: Anthropic\n");
		std::printf("  Model:    claude-sonnet-4-5\n");
		std::printf("\n  To authenticate:\n");
		std::printf("    Option A: pluto login  (Claude Max/Pro subscription)\n");
		std::printf("    Option B: export ANTHROPIC_API_KEY=sk-ant-...\n");
	} else if (choice >= 2 && choice <= (int)(confs.size() + 1)) {
		const auto& p = confs[choice - 2];
		cfg.provider = "openai-compat";
		cfg.base_url = p.base_url;
		cfg.api_key_env = p.api_key_env;
		// Use the first model as default.
		if (!p.models.empty()) {
			cfg.model = p.models[0].id;
			cfg.max_tokens = p.models[0].max_tokens;
			cfg.context_window = p.models[0].context_window;
		}
		std::printf("\n  Provider: %s\n", p.label.c_str());
		std::printf("  Model:    %s\n", cfg.model.c_str());
		if (!p.base_url.empty())
			std::printf("  Base URL: %s\n", p.base_url.c_str());
		if (!p.api_key_env.empty())
			std::printf("\n  Set your key: export %s=<your-key>\n", p.api_key_env.c_str());
		else
			std::printf("\n  No API key required.\n");
	} else {
		std::printf("\n  Invalid choice. Using default (Anthropic).\n");
		return false;
	}

	// Save config immediately.
	SaveConfig(cfg);
	std::printf("\n  Config saved to: %s\n", config_path().c_str());
	std::printf("  (change anytime with /model or settings GUI)\n\n");
	return true;
}

Config LoadConfig() {
	maybe_migrate_settings();
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
		else if (k == "system") {
			cfg.system_prompt = v;
			cfg.user_system_prompt = v;
		}
		else if (k == "max_tokens") {
			try { cfg.max_tokens = std::stoi(v); } catch (...) {}
		}
		else if (k == "context_window") {
			try { cfg.context_window = std::stoi(v); } catch (...) {}
		}
		else if (k == "cache")          cfg.cache = (v == "true" || v == "1" || v == "yes");
		else if (k == "font_delta") {
			try { cfg.font_delta = std::stoi(v); } catch (...) {}
		}
		else if (k == "max_iterations") {
			try { cfg.max_iterations = std::stoi(v); } catch (...) {}
		}
		else if (k == "color_scheme")   cfg.color_scheme = v;
		else if (k == "hook_pre_tool")    cfg.hooks_pre_tool.push_back(v);
		else if (k == "hook_post_tool")   cfg.hooks_post_tool.push_back(v);
		else if (k == "hook_user_prompt") cfg.hooks_user_prompt.push_back(v);
	}
	return cfg;
}

void AutoDetectProvider(Config& cfg) {
	const std::string& m = cfg.model;

	// OpenRouter models use "provider/model" format (e.g. "google/gemma-4-26b").
	// Detect this first — if the model ID contains a slash, it's an OpenRouter model.
	if (m.find('/') != std::string::npos) {
		cfg.provider = "openai-compat";
		cfg.base_url = "https://openrouter.ai/api";
		cfg.api_key_env = "OPENROUTER_API_KEY";
		return;
	}

	if (m.find("claude") != std::string::npos) {
		cfg.provider = "anthropic";
		cfg.base_url.clear();
		cfg.api_key_env = "ANTHROPIC_API_KEY";
	} else if (m.find("gpt-") != std::string::npos
	        || m.find("dall-e") != std::string::npos
	        || (m.size() >= 2 && m[0] == 'o' && m[1] >= '1' && m[1] <= '9')) {
		cfg.provider = "openai-compat";
		cfg.base_url = "https://api.openai.com";
		cfg.api_key_env = "OPENAI_API_KEY";
	} else if (m.find("gemini") != std::string::npos) {
		cfg.provider = "openai-compat";
		cfg.base_url = "https://generativelanguage.googleapis.com/v1beta/openai";
		cfg.api_key_env = "GEMINI_API_KEY";
	} else if (m.find("grok") != std::string::npos) {
		cfg.provider = "openai-compat";
		cfg.base_url = "https://api.x.ai";
		cfg.api_key_env = "XAI_API_KEY";
	} else if (m.find("llama") != std::string::npos
	        || m.find("mistral") != std::string::npos
	        || m.find("qwen") != std::string::npos
	        || m.find("gemma") != std::string::npos
	        || m.find("deepseek") != std::string::npos) {
		// Open-weight models could be Ollama, OpenRouter, Groq, etc.
		// Only auto-switch if currently on Anthropic (clearly wrong).
		if (cfg.provider == "anthropic") {
			cfg.provider = "openai-compat";
			cfg.base_url = "http://localhost:11434";
			cfg.api_key_env.clear();
		}
	}
}

// ---- Centralised provider factory ----

std::unique_ptr<Provider> CreateProvider(const Config& cfg) {
	std::string key = ResolveApiKey(cfg);

	if (cfg.provider == "openai-compat" && !cfg.base_url.empty())
		return std::make_unique<OpenAICompatProvider>(key, cfg.base_url);

	// Anthropic: check for OAuth tokens first.
	AuthType auth = AuthType::API_KEY;
	OAuthTokens tok = LoadOAuthTokens();
	if (!tok.empty()) {
		std::string oauth_key = OAuthEnsureValidToken(tok);
		if (!oauth_key.empty()) {
			key  = oauth_key;
			auth = AuthType::OAUTH;
		}
		// If refresh failed, fall back to API key auth.
	}
	return std::make_unique<AnthropicProvider>(key, auth);
}

int GuessMaxOutputTokens(const std::string& model) {
	if (model.find("opus") != std::string::npos)     return 32000;
	if (model.find("sonnet") != std::string::npos)   return 64000;
	if (model.find("haiku") != std::string::npos)    return 64000;
	if (model.find("claude") != std::string::npos)   return 64000;
	if (model.find("gpt-4o") != std::string::npos)   return 16384;
	if (model.find("gpt-4") != std::string::npos)    return 16384;
	if (model.find("o1") != std::string::npos)       return 100000;
	if (model.find("o3") != std::string::npos)       return 100000;
	if (model.find("o4") != std::string::npos)       return 100000;
	if (model.find("gemini") != std::string::npos)   return 65536;
	if (model.find("deepseek") != std::string::npos) return 8192;
	return 8192;
}

int GuessContextWindow(const std::string& model) {
	if (model.find("opus") != std::string::npos)     return 1000000;
	if (model.find("sonnet") != std::string::npos)   return 200000;
	if (model.find("haiku") != std::string::npos)    return 200000;
	if (model.find("claude") != std::string::npos)   return 200000;
	if (model.find("gpt-4") != std::string::npos)    return 128000;
	if (model.find("gpt-oss") != std::string::npos)  return 128000;
	if (model.find("gemini") != std::string::npos)   return 1000000;
	if (model.find("gemma-4") != std::string::npos)  return 128000;
	if (model.find("gemma-3") != std::string::npos)  return 32000;
	if (model.find("llama") != std::string::npos)    return 131072;
	if (model.find("qwen") != std::string::npos)     return 131072;
	if (model.find("deepseek") != std::string::npos) return 64000;
	if (model.find("mistral") != std::string::npos)  return 32000;
	return 128000;
}

bool SaveConfig(const Config& cfg) {
	std::string path = config_path();
	ScopedFileLock lock(path + ".lock");
	if (!lock.locked()) return false;

	// Strip newlines from string values to prevent config injection.
	auto sanitize = [](const std::string& s) -> std::string {
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			if (c != '\n' && c != '\r') out += c;
		}
		return out;
	};

	std::string tmp = path + ".tmp." + std::to_string(getpid());
	{
		std::ofstream f(tmp, std::ios::trunc);
		if (!f) return false;
		f << "provider = "       << sanitize(cfg.provider)       << "\n";
		f << "model = "          << sanitize(cfg.model)          << "\n";
		if (!cfg.base_url.empty())
			f << "base_url = "   << sanitize(cfg.base_url)       << "\n";
		f << "api_key_env = "    << sanitize(cfg.api_key_env)    << "\n";
		if (!cfg.user_system_prompt.empty())
			f << "system = "     << sanitize(cfg.user_system_prompt) << "\n";
		f << "max_tokens = "     << cfg.max_tokens     << "\n";
		f << "context_window = " << cfg.context_window << "\n";
		f << "cache = "          << (cfg.cache ? "true" : "false") << "\n";
		if (cfg.font_delta != 0)
			f << "font_delta = "  << cfg.font_delta << "\n";
		if (cfg.color_scheme != "auto")
			f << "color_scheme = " << sanitize(cfg.color_scheme) << "\n";
		for (const auto& h : cfg.hooks_pre_tool)
			f << "hook_pre_tool = " << sanitize(h) << "\n";
		for (const auto& h : cfg.hooks_post_tool)
			f << "hook_post_tool = " << sanitize(h) << "\n";
		for (const auto& h : cfg.hooks_user_prompt)
			f << "hook_user_prompt = " << sanitize(h) << "\n";
		if (!f.good()) { std::remove(tmp.c_str()); return false; }
	}
	if (::rename(tmp.c_str(), path.c_str()) != 0) {
		std::remove(tmp.c_str());
		return false;
	}
	return true;
}

namespace {

// Per-provider key file: ~/config/settings/pluto/keys/<env_var_name>
std::string provider_key_path(const std::string& env_name) {
	BPath p(settings_dir().c_str());
	p.Append("keys");
	std::string dir = p.Path();
	::mkdir(dir.c_str(), 0700);
	return dir + "/" + env_name;
}

std::string read_key_file(const std::string& path) {
	std::ifstream f(path);
	if (!f) return {};
	std::string key;
	std::getline(f, key);
	while (!key.empty()
	    && (key.back() == '\n' || key.back() == '\r'
	     || key.back() == ' '  || key.back() == '\t'))
		key.pop_back();
	return key;
}

}  // namespace

std::string ResolveProviderKey(const std::string& env_name) {
	if (env_name.empty()) return {};
	return read_key_file(provider_key_path(env_name));
}

std::string ResolveApiKey(const Config& cfg) {
	// 1. Environment variable.
	const char* v = std::getenv(cfg.api_key_env.c_str());
	if (v && *v) return std::string(v);

	// 2. Per-provider key file.
	if (!cfg.api_key_env.empty()) {
		std::string key = read_key_file(provider_key_path(cfg.api_key_env));
		if (!key.empty()) return key;
	}

	// 3. Legacy single key file (fallback).
	return read_key_file(api_key_path());
}

namespace {

// Read a whole file into a string. Returns empty if missing.
std::string read_file_to_string(const std::string& path) {
	std::ifstream f(path);
	if (!f) return {};
	std::ostringstream os;
	os << f.rdbuf();
	std::string s = os.str();
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
		s.pop_back();
	return s;
}

// Append a single file to cfg.system_prompt wrapped in a tag.
void append_doc(Config& cfg, const std::string& path, const char* tag) {
	std::string body = read_file_to_string(path);
	if (body.empty()) return;
	if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
	cfg.system_prompt += "<";
	cfg.system_prompt += tag;
	cfg.system_prompt += " source=\"";
	cfg.system_prompt += path;
	cfg.system_prompt += "\">\n";
	cfg.system_prompt += body;
	cfg.system_prompt += "\n</";
	cfg.system_prompt += tag;
	cfg.system_prompt += ">";
}

}  // namespace

void ApplyProjectMemory(Config& cfg) {
	// Prepend a base system prompt if the user hasn't set a custom one.
	// This tells the model about its capabilities and workspace.
	if (cfg.system_prompt.empty()) {
		char cwd[PATH_MAX] = {};
		::getcwd(cwd, sizeof(cwd));
		cfg.system_prompt =
			"You are Pluto, a Haiku-native AI coding assistant. "
			"You have access to tools that let you read, write, edit, "
			"and search files in the user's workspace, run shell commands, "
			"and query the file system. Use these tools proactively to "
			"answer questions about the project — do not ask the user to "
			"provide files you can read yourself.\n\n"
			"Current workspace: " + std::string(cwd);
	}

	append_doc(cfg, home_dir() + "/config/settings/pluto/pluto.md",
		"user_memory");
	append_doc(cfg, "./pluto.md", "project_memory");
}

// Parse optional YAML-ish frontmatter from a skill file.
// Returns the body (after frontmatter) and fills triggers if present.
static std::string parse_skill_frontmatter(const std::string& content,
                                            std::vector<std::string>& triggers) {
	triggers.clear();
	Frontmatter fm = ParseFrontmatter(content);
	// Accept both "trigger" and "triggers" as key.
	if (fm.has("trigger"))
		triggers = fm.get_list("trigger");
	else if (fm.has("triggers"))
		triggers = fm.get_list("triggers");
	return fm.body;
}

void ApplySkills(Config& cfg) {
	std::string skills_dir = home_dir() + "/config/settings/pluto/skills";
	BDirectory bdir(skills_dir.c_str());
	if (bdir.InitCheck() != B_OK) return;

	std::string always_block;
	int always_count = 0;
	std::string catalog;
	int triggered_count = 0;

	BEntry entry;
	while (bdir.GetNextEntry(&entry) == B_OK) {
		char nameBuf[B_FILE_NAME_LENGTH];
		if (entry.GetName(nameBuf) != B_OK) continue;
		std::string name = nameBuf;
		std::string skill_path = skills_dir + "/" + name + "/SKILL.md";
		std::string raw = read_file_to_string(skill_path);
		if (raw.empty()) continue;

		std::vector<std::string> triggers;
		std::string body = parse_skill_frontmatter(raw, triggers);

		if (triggers.empty()) {
			// Always-on skill (backward compatible).
			// XML-escape the skill name to prevent injection.
			std::string safe_name;
			for (char c : name) {
				if (c == '<') safe_name += "&lt;";
				else if (c == '>') safe_name += "&gt;";
				else if (c == '&') safe_name += "&amp;";
				else if (c == '"') safe_name += "&quot;";
				else safe_name += c;
			}
			always_block += "\n\n<skill name=\"";
			always_block += safe_name;
			always_block += "\">\n";
			always_block += body;
			always_block += "\n</skill>";
			++always_count;
		} else {
			// Triggered skill — store for on-demand injection.
			SkillEntry se;
			se.name = name;
			se.body = body;
			se.triggers = triggers;
			se.always = false;
			cfg.triggered_skills.push_back(std::move(se));

			// Add to catalog for visibility (XML-escape name).
			std::string cat_name;
			for (char c : name) {
				if (c == '<') cat_name += "&lt;";
				else if (c == '>') cat_name += "&gt;";
				else if (c == '&') cat_name += "&amp;";
				else cat_name += c;
			}
			catalog += "\n  - ";
			catalog += cat_name;
			catalog += " (triggers: ";
			for (size_t i = 0; i < triggers.size(); ++i) {
				if (i > 0) catalog += ", ";
				catalog += triggers[i];
			}
			catalog += ")";
			++triggered_count;
		}
	}

	if (always_count > 0) {
		if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
		cfg.system_prompt += "<skills>";
		cfg.system_prompt += always_block;
		cfg.system_prompt += "\n</skills>";
	}

	if (triggered_count > 0) {
		if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
		cfg.system_prompt += "<available_skills>";
		cfg.system_prompt += "\nThe following skills can be activated when relevant:";
		cfg.system_prompt += catalog;
		cfg.system_prompt += "\n</available_skills>";
	}
}

std::vector<std::string> InjectTriggeredSkills(Config& cfg,
                                                  const std::string& user_message) {
	std::vector<std::string> activated;
	if (cfg.triggered_skills.empty()) return activated;

	// Case-insensitive search in the user message.
	std::string lower_msg = user_message;
	for (auto& c : lower_msg) c = std::tolower(static_cast<unsigned char>(c));

	auto it = cfg.triggered_skills.begin();
	while (it != cfg.triggered_skills.end()) {
		bool matched = false;
		for (const auto& trigger : it->triggers) {
			// Triggers are typically short (1-3 words); tolower inline.
			std::string lt;
			lt.reserve(trigger.size());
			for (char c : trigger)
				lt += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (lower_msg.find(lt) != std::string::npos) {
				matched = true;
				break;
			}
		}

		if (matched) {
			// Inject into system prompt.
			if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
			cfg.system_prompt += "<skill name=\"";
			cfg.system_prompt += it->name;
			cfg.system_prompt += "\">\n";
			cfg.system_prompt += it->body;
			cfg.system_prompt += "\n</skill>";

			activated.push_back(it->name);
			it = cfg.triggered_skills.erase(it);
		} else {
			++it;
		}
	}
	return activated;
}

namespace {

bool write_key_file(const std::string& path, const std::string& key) {
	ScopedFileLock lock(path + ".lock");
	if (!lock.locked()) return false;

	std::string tmp = path + ".tmp." + std::to_string(getpid());
	// Create with restricted permissions from the start to avoid
	// a TOCTOU window where another process could read the key.
	int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) return false;
	std::string data = key + "\n";
	ssize_t written = ::write(fd, data.c_str(), data.size());
	::close(fd);
	if (written != (ssize_t)data.size()) {
		std::remove(tmp.c_str());
		return false;
	}
	if (::rename(tmp.c_str(), path.c_str()) != 0) {
		std::remove(tmp.c_str());
		return false;
	}
	return true;
}

}  // namespace

bool SaveApiKey(const std::string& key) {
	return write_key_file(api_key_path(), key);
}

bool SaveApiKey(const std::string& key, const std::string& env_name) {
	if (env_name.empty()) return SaveApiKey(key);
	return write_key_file(provider_key_path(env_name), key);
}

Credentials ResolveCredentials(Config& cfg) {
	// 1. Check for OAuth tokens (only relevant for Anthropic provider).
	if (cfg.provider == "anthropic") {
		OAuthTokens tokens = LoadOAuthTokens();
		if (!tokens.empty()) {
			std::string at = OAuthEnsureValidToken(tokens);
			if (!at.empty())
				return { Credentials::OAUTH, at };
			// OAuth tokens exist but refresh failed — do NOT fall through to
			// API key silently. The user intended to use their subscription.
			std::fprintf(stderr,
				"[auth] OAuth token refresh failed. "
				"Run 'pluto login' to re-authenticate.\n"
				"       (Not falling back to API key to avoid unexpected billing.)\n");
			return {};
		}
	}

	// 2. API key (for any provider). Local servers (empty api_key_env)
	//    don't need credentials — return a dummy token.
	if (cfg.api_key_env.empty())
		return { Credentials::API_KEY, "local" };

	std::string key = ResolveApiKey(cfg);
	if (!key.empty())
		return { Credentials::API_KEY, key };

	return {};
}

void ApplyRagHint(Config& cfg) {
	if (RagEntryCount() == 0) return;
	if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
	cfg.system_prompt +=
		"<beapi_hint>\n"
		"You have access to a local BeAPI reference with "
		+ std::to_string(RagEntryCount()) +
		" code examples covering Haiku OS APIs.\n"
		"When writing Haiku-native C++ code, use the `beapi_lookup` tool "
		"to find correct API usage patterns for Be classes "
		"(BView, BWindow, BMessage, BSlider, etc.).\n"
		"Relevant snippets are also automatically injected when your user "
		"mentions Be class names.\n"
		"</beapi_hint>";
}

// Parse mode frontmatter from a .md file.
static ModeEntry parse_mode_file(const std::string& content) {
	ModeEntry m;
	Frontmatter fm = ParseFrontmatter(content);
	if (fm.fields.empty())
		return m;
	m.name = fm.get("name");
	m.description = fm.get("description");
	m.model = fm.get("model");
	m.disabled_tools = fm.get_list("disabled_tools");
	m.prompt = fm.body;
	return m;
}

void LoadModes(Config& cfg) {
	cfg.modes.clear();

	// Built-in "plan" mode (backward compatible with old plan_mode toggle).
	ModeEntry plan;
	plan.name = "plan";
	plan.description = "Read-only analysis — no file writes";
	plan.prompt = "You are in plan/analysis mode. "
		"You can read files and search but cannot modify anything. "
		"Describe what changes you would make without making them.";
	plan.disabled_tools = {
		"write_file", "edit_file", "apply_patch", "bash",
		"write_attr", "remove_attr", "desktop_script", "launch_app"
	};
	cfg.modes.push_back(std::move(plan));

	// Load user-defined modes from ~/config/settings/pluto/modes/*.md
	std::string modes_dir = home_dir() + "/config/settings/pluto/modes";
	BDirectory modesDir(modes_dir.c_str());
	if (modesDir.InitCheck() != B_OK) return;

	BEntry modeEntry;
	while (modesDir.GetNextEntry(&modeEntry) == B_OK) {
		char nameBuf[B_FILE_NAME_LENGTH];
		if (modeEntry.GetName(nameBuf) != B_OK) continue;
		std::string fname = nameBuf;
		if (fname.size() < 4
			|| fname.substr(fname.size() - 3) != ".md")
			continue;
		std::string raw = read_file_to_string(modes_dir + "/" + fname);
		if (raw.empty()) continue;
		ModeEntry m = parse_mode_file(raw);
		if (m.name.empty()) {
			// Use filename (without .md) as name.
			m.name = fname.substr(0, fname.size() - 3);
		}
		// Don't override the built-in plan mode.
		bool duplicate = false;
		for (const auto& existing : cfg.modes) {
			if (existing.name == m.name) { duplicate = true; break; }
		}
		if (!duplicate)
			cfg.modes.push_back(std::move(m));
	}
}

const ModeEntry* FindMode(const Config& cfg, const std::string& name) {
	for (const auto& m : cfg.modes) {
		if (m.name == name) return &m;
	}
	return nullptr;
}

}  // namespace pluto
