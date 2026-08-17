#include "repl.h"

#include "agent/Loop.h"
#include "cancel.h"
#include "input.h"
#include "json.h"
#include "markdown.h"
#include "routine.h"
#include "telemetry.h"
#include "util.h"
#include "ipc/PlutoMessenger.h"
#include "mcp/SseSubscriber.h"
#include "storage/SessionStore.h"
#include "workspace.h"
#include "map/Graph.h"
#include "map/Scanner.h"
#include "map/BfsAttrs.h"

#include <Clipboard.h>
#include <Message.h>

#include <curl/curl.h>
#include <dirent.h>
#include <histedit.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace pluto {

namespace {

const char* kReset = "\x1b[0m";
const char* kDim   = "\x1b[2m";
const char* kBold  = "\x1b[1m";
const char* kCyan  = "\x1b[36m";
const char* kGreen = "\x1b[32m";
const char* kRed   = "\x1b[31m";
const char* kMag   = "\x1b[35m";

// --- libedit (editline) wrapper ---

std::string history_path() {
	std::string dir = SettingsDir();
	mkdir(dir.c_str(), 0755);
	return dir + "/history";
}

// Prompt callback required by editline.
const char* el_prompt_cb(EditLine* /*el*/) {
	static std::string prompt_str;
	prompt_str = std::string(kCyan) + "> " + kReset;
	return prompt_str.c_str();
}

// Global editline state (initialized in RunRepl, used by read_line).
EditLine*  g_el  = nullptr;
History*   g_hist = nullptr;
HistEvent  g_hev;

// Slash command completion for Tab key.
static const char* kSlashCommands[] = {
	"/ask", "/commands", "/context", "/copy", "/cost", "/delegate",
	"/delete", "/events", "/help", "/hotkeys", "/init", "/list", "/load",
	"/map", "/mode", "/model", "/models", "/modes",
	"/peers", "/plan", "/quit", "/reset", "/resume",
	"/routine", "/routines", "/save", "/share", "/tag", "/title",
	"/tools", "/untag", "/unwatch", "/watch",
	nullptr
};

unsigned char slash_complete(EditLine* el, int /*ch*/) {
	const LineInfo* li = el_line(el);
	if (!li) return CC_ERROR;

	// Only complete if the line starts with '/'.
	size_t len = static_cast<size_t>(li->cursor - li->buffer);
	if (len == 0 || li->buffer[0] != '/')
		return CC_ERROR;

	std::string prefix(li->buffer, len);

	// Find all matching commands.
	std::vector<const char*> matches;
	for (const char** p = kSlashCommands; *p; ++p) {
		if (std::string(*p).rfind(prefix, 0) == 0)
			matches.push_back(*p);
	}

	if (matches.empty())
		return CC_ERROR;

	if (matches.size() == 1) {
		// Single match — insert the remaining characters + space.
		const char* suffix = matches[0] + len;
		el_insertstr(el, suffix);
		el_insertstr(el, " ");
		return CC_REFRESH;
	}

	// Multiple matches — find longest common prefix and complete to it.
	std::string common = matches[0];
	for (size_t i = 1; i < matches.size(); ++i) {
		size_t j = 0;
		while (j < common.size() && matches[i][j] == common[j]) ++j;
		common.resize(j);
	}
	if (common.size() > len) {
		std::string suffix = common.substr(len);
		el_insertstr(el, suffix.c_str());
		return CC_REFRESH;
	}

	// Show all possibilities.
	std::fprintf(stdout, "\n");
	for (const char* m : matches)
		std::fprintf(stdout, "  %s\n", m);
	return CC_REDISPLAY;
}

bool read_line(std::string& out) {
	out.clear();
	if (g_el) {
		int count = 0;
		const char* line = el_gets(g_el, &count);
		if (!line || count <= 0) return false;
		out.assign(line, count);
		while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
			out.pop_back();
		// Add non-empty, non-slash-only lines to history.
		if (!out.empty() && g_hist) {
			history(g_hist, &g_hev, H_ENTER, out.c_str());
		}
		return true;
	}
	// Fallback: plain fgets (e.g. if editline init failed).
	char buf[4096];
	if (!std::fgets(buf, sizeof(buf), stdin)) return false;
	out = buf;
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
		out.pop_back();
	return true;
}

std::string first_line(const std::string& s) {
	size_t nl = s.find('\n');
	return nl == std::string::npos ? s : s.substr(0, nl);
}

std::string truncate(const std::string& s, size_t n) {
	return s.size() <= n ? s : s.substr(0, n) + "…";
}

std::string format_date(int64_t unix_ts) {
	if (unix_ts <= 0) return "           ";
	std::time_t t = static_cast<std::time_t>(unix_ts);
	std::tm tm;
	if (!localtime_r(&t, &tm)) return "           ";
	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
	return buf;
}

std::string derive_title(const std::string& first_user_text) {
	std::string t = first_line(first_user_text);
	if (t.size() > 60) { t.resize(57); t += "…"; }
	return t;
}

// ---------------- custom slash commands ----------------

struct CustomCommand {
	std::string name;          // file basename without .md
	std::string description;   // from frontmatter, if any
	std::string body;          // template, with {{args}} substitution
};

std::string commands_dir() {
	return SettingsDir() + "/commands";
}

// Parse optional YAML-ish frontmatter (--- ... ---) at the top of `text`.
// Recognizes only `description: ...`. Mutates text to point past the
// frontmatter and stores the description in `desc` if found.
void parse_frontmatter(std::string& text, std::string& desc) {
	if (text.size() < 4 || text.substr(0, 4) != "---\n") return;
	size_t end = text.find("\n---\n", 4);
	if (end == std::string::npos) return;
	std::string fm = text.substr(4, end - 4);
	size_t dpos = fm.find("description:");
	if (dpos != std::string::npos) {
		size_t nl = fm.find('\n', dpos);
		std::string d = fm.substr(dpos + 12,
			(nl == std::string::npos ? fm.size() : nl) - dpos - 12);
		while (!d.empty() && (d.front() == ' ' || d.front() == '\t'))
			d.erase(0, 1);
		while (!d.empty() && (d.back() == ' ' || d.back() == '\r'))
			d.pop_back();
		desc = d;
	}
	text = text.substr(end + 5);
}

std::vector<CustomCommand> load_custom_commands() {
	std::vector<CustomCommand> out;
	std::string dir = commands_dir();
	DIR* d = opendir(dir.c_str());
	if (!d) return out;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		std::string name = e->d_name;
		if (name.size() < 4) continue;
		if (name.substr(name.size() - 3) != ".md") continue;
		std::string path = dir + "/" + name;
		std::ifstream f(path);
		if (!f) continue;
		std::ostringstream os;
		os << f.rdbuf();
		CustomCommand c;
		c.name = name.substr(0, name.size() - 3);
		c.body = os.str();
		parse_frontmatter(c.body, c.description);
		out.push_back(std::move(c));
	}
	closedir(d);
	return out;
}

// If `line` starts with /<name> and matches a custom command, fill
// `expanded` with the body (with {{args}} substituted) and return true.
bool try_custom_command(const std::vector<CustomCommand>& cmds,
                        const std::string& line,
                        std::string& expanded) {
	if (line.empty() || line[0] != '/') return false;
	size_t end = line.find(' ');
	std::string name = line.substr(1, end == std::string::npos
		? std::string::npos : end - 1);
	std::string args = end == std::string::npos
		? std::string() : line.substr(end + 1);
	for (const auto& c : cmds) {
		if (c.name != name) continue;
		expanded = c.body;
		// Substitute {{args}} → args
		size_t pos = 0;
		while ((pos = expanded.find("{{args}}", pos)) != std::string::npos) {
			expanded.replace(pos, 8, args);
			pos += args.size();
		}
		return true;
	}
	return false;
}

// -------- /models: fetch available models from an Ollama/OpenAI server --------

struct RemoteModel {
	std::string name;
	std::string family;
	std::string param_size;
	std::string quant;
	int64_t     size_bytes = 0;
};

static size_t collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	static_cast<std::string*>(ud)->append(ptr, size * nmemb);
	return size * nmemb;
}

// Try Ollama /api/tags first; fall back to OpenAI /v1/models.
std::vector<RemoteModel> fetch_remote_models(const std::string& base_url,
                                              std::string& err) {
	std::vector<RemoteModel> out;

	CURL* curl = curl_easy_init();
	if (!curl) { err = "curl_easy_init failed"; return out; }

	// --- try Ollama endpoint first ---
	std::string body;
	std::string url = base_url;
	while (!url.empty() && url.back() == '/') url.pop_back();
	std::string ollama_url = url + "/api/tags";

	curl_easy_setopt(curl, CURLOPT_URL, ollama_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "pluto/0.3.0 (Haiku)");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (rc == CURLE_OK && http_code == 200) {
		// Parse Ollama JSON: { "models": [ { "name", "details": { ... } } ] }
		try {
			json::Value root = json::Value::parse(body);
			const json::Value* models = root.find("models");
			if (models && models->is_arr()) {
				for (const auto& m : models->as_arr()) {
					RemoteModel rm;
					rm.name = m.str("name");
					const json::Value* sz = m.find("size");
					if (sz && sz->is_num()) rm.size_bytes = (int64_t)sz->as_num();
					const json::Value* det = m.find("details");
					if (det && det->is_obj()) {
						rm.family     = det->str("family");
						rm.param_size = det->str("parameter_size");
						rm.quant      = det->str("quantization_level");
					}
					if (!rm.name.empty()) out.push_back(std::move(rm));
				}
			}
		} catch (const std::exception&) {
			err = "failed to parse Ollama response";
		}
		curl_easy_cleanup(curl);
		return out;
	}

	// --- fallback: OpenAI /v1/models ---
	body.clear();
	std::string oai_url = url + "/v1/models";
	curl_easy_setopt(curl, CURLOPT_URL, oai_url.c_str());
	rc = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		err = std::string("connection failed: ") + curl_easy_strerror(rc);
		return out;
	}
	if (http_code != 200) {
		err = "HTTP " + std::to_string(http_code);
		return out;
	}

	try {
		json::Value root = json::Value::parse(body);
		const json::Value* data = root.find("data");
		if (data && data->is_arr()) {
			for (const auto& m : data->as_arr()) {
				RemoteModel rm;
				rm.name = m.str("id");
				if (!rm.name.empty()) out.push_back(std::move(rm));
			}
		}
	} catch (const std::exception&) {
		err = "failed to parse /v1/models response";
	}
	return out;
}

std::string human_size(int64_t bytes) {
	if (bytes <= 0) return "";
	double gb = bytes / (1024.0 * 1024.0 * 1024.0);
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.1f GB", gb);
	return buf;
}

void cmd_tools(const ToolRegistry& tools, const Config& cfg) {
	// If a mode is active, show which tools are disabled.
	std::set<std::string> disabled;
	const ModeEntry* mode = nullptr;
	if (!cfg.active_mode.empty()) {
		mode = FindMode(cfg, cfg.active_mode);
		if (mode) {
			for (const auto& t : mode->disabled_tools)
				disabled.insert(t);
		}
	}
	for (const Tool* t : tools.list()) {
		bool hidden = disabled.count(t->name()) > 0;
		std::string desc = first_line(t->description());
		if (desc.size() > 80) { desc.resize(77); desc += "..."; }
		if (hidden) {
			std::printf("  %s%-24s  %s (disabled by %s mode)%s\n",
				kDim, t->name().c_str(), desc.c_str(),
				cfg.active_mode.c_str(), kReset);
		} else {
			std::printf("  %s%-24s%s  %s\n",
				kBold, t->name().c_str(), kReset, desc.c_str());
		}
	}
}

}  // namespace

int RunRepl(std::unique_ptr<Provider>& provider, Config& cfg,
             ToolRegistry& tools, bool yolo) {
	std::printf("%sPluto%s %s(%s / %s%s%s)  %zu tools%s  type /help or /quit\n",
		kBold, kReset, kDim, provider->name().c_str(), kBold, cfg.model.c_str(),
		kDim, tools.list().size(), kReset);
	if (yolo) std::printf("%s(yolo: tool calls auto-approved)%s\n", kRed, kReset);

	// --- Initialize libedit (editline) ---
	g_hist = history_init();
	if (g_hist) {
		history(g_hist, &g_hev, H_SETSIZE, 500);
		// Load saved history from disk.
		std::string hpath = history_path();
		history(g_hist, &g_hev, H_LOAD, hpath.c_str());
	}
	g_el = el_init("pluto", stdin, stdout, stderr);
	if (g_el) {
		el_set(g_el, EL_PROMPT, el_prompt_cb);
		el_set(g_el, EL_EDITOR, "emacs");   // emacs keybindings (Ctrl-A/E/K/W)
		if (g_hist) el_set(g_el, EL_HIST, history, g_hist);
		el_set(g_el, EL_SIGNAL, 1);         // handle window resize
		// Tab completion for slash commands.
		el_set(g_el, EL_ADDFN, "slash-complete",
			"Complete slash commands", slash_complete);
		el_set(g_el, EL_BIND, "\t", "slash-complete", NULL);
	}

	// Cleanup editline on scope exit.
	struct ElCleanup {
		~ElCleanup() {
			if (g_hist) {
				std::string hpath = history_path();
				history(g_hist, &g_hev, H_SAVE, hpath.c_str());
				history_end(g_hist);
				g_hist = nullptr;
			}
			if (g_el) {
				el_end(g_el);
				g_el = nullptr;
			}
		}
	} el_cleanup;

	// First-run workspace trust prompt (skip in yolo mode).
	if (!yolo && !WorkspaceRoot().empty()
	    && !IsWorkspaceTrusted(WorkspaceRoot())
	    && !IsSessionTrusted()) {
		std::printf("\n%s\xe2\x9a\xa0 First time in %s%s\n",
			kRed, WorkspaceRoot().c_str(), kReset);
		std::printf("  Authorize pluto to work here? [y]once [a]always [n]o: ");
		std::fflush(stdout);
		char buf[32];
		if (std::fgets(buf, sizeof(buf), stdin)) {
			char c = 0;
			for (char* p = buf; *p; ++p) {
				if (!std::isspace(static_cast<unsigned char>(*p))) {
					c = static_cast<char>(
						std::tolower(static_cast<unsigned char>(*p)));
					break;
				}
			}
			if (c == 'a') {
				TrustWorkspace(WorkspaceRoot());
				std::printf("%s  (workspace trusted permanently)%s\n", kDim, kReset);
			} else if (c == 'n') {
				std::printf("%s  (tools will error on file access)%s\n", kDim, kReset);
			} else {
				// 'y' or default — session-only trust
				TrustSession();
				std::printf("%s  (trusted for this session)%s\n", kDim, kReset);
			}
		}
		std::printf("\n");
	}

	// Workspace collision detection: warn if another Pluto instance is
	// already working in the same directory.
	{
		auto collision = ipc::check_workspace_collision(WorkspaceRoot());
		if (collision.team_id >= 0) {
			std::printf("%s\xe2\x9a\xa0 Another Pluto instance (PID %d) is already "
				"working in %s%s\n",
				kRed, (int)collision.team_id,
				WorkspaceRoot().c_str(), kReset);
			std::printf("  %smodel: %s, %s%s\n", kDim,
				collision.model.empty() ? "?" : collision.model.c_str(),
				collision.busy ? "busy" : "idle", kReset);
		}
	}

	std::vector<Message> messages;
	std::string line;

	long long session_input_tokens = 0;
	long long session_output_tokens = 0;
	int turns = 0;

	std::string current_session_path;   // empty until first /save
	std::string current_title;          // auto-derived or user-set

	auto custom_commands = load_custom_commands();

	std::unique_ptr<SseSubscriber> subscriber;

	TerminalMarkdown md;

	UiCallbacks ui;
	ui.on_text = [&md](const std::string& text) {
		std::printf("%s", kGreen);
		std::string rendered = md.Feed(text);
		if (!rendered.empty())
			std::fwrite(rendered.data(), 1, rendered.size(), stdout);
		std::printf("%s", kReset);
		std::fflush(stdout);
	};
	ui.on_tool_start = [](const std::string&, const std::string& preview) {
		std::printf("\n%s◆ %s%s\n", kMag, preview.c_str(), kReset);
	};
	ui.on_tool_result = [](const std::string&, const std::string&,
	                       const ToolResult& r) {
		const char* tag = r.is_error ? kRed : kDim;
		std::string out = r.output;
		if (out.size() > 500) out = out.substr(0, 500) + "\n... [truncated]";
		std::printf("%s", tag);
		size_t start = 0;
		while (start < out.size()) {
			size_t nl = out.find('\n', start);
			size_t end = (nl == std::string::npos) ? out.size() : nl;
			std::printf("  %s\n", out.substr(start, end - start).c_str());
			start = (nl == std::string::npos) ? out.size() : nl + 1;
		}
		std::printf("%s", kReset);
	};
	ui.on_error = [](const std::string& err) {
		std::printf("%s[error] %s%s\n", kRed, err.c_str(), kReset);
	};
	ui.on_usage = [&](int in_tok, int out_tok,
	                  int cache_read, int cache_creation) {
		session_input_tokens  += in_tok;
		session_output_tokens += out_tok;
		++turns;
		TelemetryRecord(cfg.model, in_tok, out_tok, cache_read, cache_creation);
	};
	ui.on_status = [](const std::string& status) {
		bool warn = status.find("does not support tools") != std::string::npos;
		const char* color = warn ? "\x1b[33m" : kDim;  // yellow for warnings
		const char* icon  = warn ? "\xe2\x9a\xa0 " : "";
		std::fprintf(stderr, "%s%s[pluto] %s%s\n",
			color, icon, status.c_str(), kReset);
	};

	PermissionFn perm = yolo ? AutoAllow() : InteractivePrompt();

	for (;;) {
		if (!g_el) {
			// Fallback prompt when editline is unavailable.
			std::printf("%s> %s", kCyan, kReset);
			std::fflush(stdout);
		}
		if (!read_line(line)) { std::printf("\n"); break; }
		if (line.empty()) continue;

		if (line == "/quit" || line == "/exit") break;
		if (line == "/help") {
			std::printf("  /quit                exit\n");
			std::printf("  /mode [name]         switch mode (plan, review, etc) or show current\n");
			std::printf("  /modes               list available modes\n");
			std::printf("  /plan                shortcut for /mode plan (toggle)\n");
			std::printf("  /reset               clear conversation history\n");
			std::printf("  /tools               list registered tools\n");
			std::printf("  /model ID            switch model for subsequent turns\n");
			std::printf("  /models [URL]        list models on Ollama/OpenAI server\n");
			std::printf("  /cost                show session token totals\n");
			std::printf("  /copy                copy last assistant reply to clipboard\n");
			std::printf("  /hotkeys             list REPL key bindings\n");
			std::printf("  /save [title]        save session to BFS store\n");
			std::printf("  /share [path]        export conversation as Markdown file\n");
			std::printf("  /load <query>        load a session by title or index\n");
			std::printf("  /list                list saved sessions\n");
			std::printf("  /resume              load most recent session\n");
			std::printf("  /delete              delete the current session file\n");
			std::printf("  /title <new>         set session title (used on next /save)\n");
			std::printf("  /tag [name]          show tags or add one to current session\n");
			std::printf("  /untag <name>        remove a tag from current session\n");
			std::printf("  /routines            list available routines\n");
			std::printf("  /routine <name>      run a routine in this chat\n");
			std::printf("  /commands            list user-defined slash commands\n");
			std::printf("  @<path>              inline a file's contents in the prompt\n");
			std::printf("  /watch [URL]         subscribe to an SSE stream\n");
			std::printf("                       (default http://localhost:2607/sse)\n");
			std::printf("  /unwatch             stop the SSE subscription\n");
			std::printf("  /events              show pending SSE events without\n");
			std::printf("                       sending a new turn\n");
			std::printf("  /peers               list running Pluto GUI instances\n");
			std::printf("  /ask <peer> <text>   send a prompt to a Pluto GUI peer\n");
			std::printf("  /delegate <peer> <t> fire-and-forget prompt to peer (notifies)\n");
			std::printf("  /context <peer> <t>  ask a peer what it knows about a topic\n");
			std::printf("  /init [--force]      create pluto.md project memory in workspace\n");
			std::printf("  /map            scan workspace and build knowledge graph\n");
			std::printf("  /map path A B   find shortest path between two nodes\n");
			std::printf("  /map query ...  query the graph with natural language\n");
			continue;
		}
		if (line == "/reset") {
			messages.clear();
			current_session_path.clear();
			current_title.clear();
			session_input_tokens = 0;
			session_output_tokens = 0;
			turns = 0;
			std::printf("%s(history cleared)%s\n", kDim, kReset);
			continue;
		}
		if (line == "/plan") {
			// Toggle shortcut: /plan == toggle "plan" mode on/off.
			if (cfg.active_mode == "plan")
				cfg.active_mode.clear();
			else
				cfg.active_mode = "plan";
			const ModeEntry* m = FindMode(cfg, "plan");
			std::printf("%s(plan mode %s — %s)%s\n",
				kDim,
				cfg.active_mode == "plan" ? "ON" : "OFF",
				cfg.active_mode == "plan" ? "read-only analysis, no file writes"
				                          : "full tool access restored",
				kReset);
			(void)m;
			continue;
		}
		if (line == "/modes") {
			if (cfg.modes.empty()) {
				std::printf("%s(no modes available)%s\n", kDim, kReset);
			} else {
				for (const auto& m : cfg.modes) {
					const char* mark = (m.name == cfg.active_mode) ? "* " : "  ";
					std::printf("%s%s%-16s%s %s%s%s\n",
						mark,
						(m.name == cfg.active_mode) ? "\033[1m" : "",
						m.name.c_str(),
						(m.name == cfg.active_mode) ? "\033[0m" : "",
						kDim, m.description.c_str(), kReset);
				}
			}
			continue;
		}
		if (line.rfind("/mode", 0) == 0 && (line.size() == 5 || line[5] == ' ')) {
			std::string arg;
			if (line.size() > 6) arg = line.substr(6);
			// Trim
			while (!arg.empty() && arg[0] == ' ') arg.erase(0, 1);
			while (!arg.empty() && arg.back() == ' ') arg.pop_back();

			if (arg.empty()) {
				// Show current mode.
				if (cfg.active_mode.empty()) {
					std::printf("%s(no mode active — full tool access)%s\n",
						kDim, kReset);
				} else {
					const ModeEntry* m = FindMode(cfg, cfg.active_mode);
					std::printf("%s(mode: %s — %s)%s\n",
						kDim, cfg.active_mode.c_str(),
						m ? m->description.c_str() : "unknown",
						kReset);
				}
			} else if (arg == "off" || arg == "none" || arg == "default") {
				cfg.active_mode.clear();
				std::printf("%s(mode cleared — full tool access)%s\n",
					kDim, kReset);
			} else {
				const ModeEntry* m = FindMode(cfg, arg);
				if (!m) {
					std::printf("%s(unknown mode \"%s\" — use /modes to list)%s\n",
						kDim, arg.c_str(), kReset);
				} else if (cfg.active_mode == arg) {
					// Toggle off if already active.
					cfg.active_mode.clear();
					std::printf("%s(mode %s OFF — full tool access)%s\n",
						kDim, arg.c_str(), kReset);
				} else {
					cfg.active_mode = arg;
					std::printf("%s(mode: %s — %s)%s\n",
						kDim, m->name.c_str(), m->description.c_str(),
						kReset);
					if (!m->model.empty()) {
						cfg.model = m->model;
						AutoDetectProvider(cfg);
						std::printf("%s  model → %s%s\n",
							kDim, m->model.c_str(), kReset);
					}
				}
			}
			continue;
		}

		if (line.rfind("/init", 0) == 0 && (line.size() == 5 || line[5] == ' ')) {
			std::string root = WorkspaceRoot();
			if (root.empty()) {
				char buf[PATH_MAX];
				if (getcwd(buf, sizeof(buf))) root = buf;
			}
			bool force = (line.find("--force") != std::string::npos);

			std::printf("%s  detecting project info…%s\n", kDim, kReset);
			std::string err;
			ProjectInfo info;
			std::string md_path = InitProjectMemory(root, force, err, info);
			if (md_path.empty()) {
				if (err.find("already exists") != std::string::npos) {
					std::printf("  %spluto.md already exists%s (%s/pluto.md)\n",
						kBold, kReset, root.c_str());
					std::printf("  use %s/init --force%s to overwrite\n",
						kBold, kReset);
				} else {
					std::printf("%s[init] %s%s\n", kRed, err.c_str(), kReset);
				}
				continue;
			}

			std::printf("  %screated%s %s\n", kGreen, kReset, md_path.c_str());
			if (!info.main_language.empty())
				std::printf("  %sdetected: %s%s\n",
					kDim, info.main_language.c_str(), kReset);
			if (!info.interesting_files.empty()) {
				std::printf("  %sbuild: ", kDim);
				for (size_t i = 0; i < info.interesting_files.size(); ++i) {
					if (i > 0) std::printf(", ");
					std::printf("%s", info.interesting_files[i].c_str());
				}
				std::printf("%s\n", kReset);
			}
			std::printf("\n  Edit %s to add project-specific instructions.\n",
				md_path.c_str());
			std::printf("  It will be loaded automatically on the next session.\n");

			ApplyProjectMemory(cfg);
			std::printf("  %s(project memory reloaded)%s\n", kDim, kReset);
			continue;
		}

		if (line.rfind("/map", 0) == 0 && (line.size() == 4 || line[4] == ' ')) {
			std::string root = WorkspaceRoot();
			if (root.empty()) {
				char buf[PATH_MAX];
				if (getcwd(buf, sizeof(buf))) root = buf;
			}

			std::string arg;
			if (line.size() > 4) {
				arg = line.substr(4);
				while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
			}

			std::string graph_path = root + "/graph.json";

			if (arg.rfind("path ", 0) == 0) {
				// /map path A B
				auto graph = map::Graph::LoadFromFile(graph_path);
				if (graph.Nodes().empty()) {
					std::printf("%s  no graph found — run /map first%s\n",
						kRed, kReset);
					continue;
				}
				std::string rest = arg.substr(5);
				while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
				auto sp = rest.find(' ');
				if (sp == std::string::npos) {
					std::printf("  usage: /map path <from> <to>\n");
					continue;
				}
				std::string from_pat = rest.substr(0, sp);
				std::string to_pat = rest.substr(sp + 1);
				while (!to_pat.empty() && to_pat.front() == ' ') to_pat.erase(0, 1);

				auto from_nodes = graph.FindNodes(from_pat);
				auto to_nodes = graph.FindNodes(to_pat);
				if (from_nodes.empty() || to_nodes.empty()) {
					std::printf("  node not found\n");
					continue;
				}
				auto path = graph.PathBetween(from_nodes[0]->id, to_nodes[0]->id);
				if (path.empty()) {
					std::printf("  no path found\n");
					continue;
				}
				std::printf("\n  %sPath (%zu hops):%s\n", kBold, path.size() - 1, kReset);
				for (size_t i = 0; i < path.size(); ++i) {
					std::printf("  %s%s%s%s\n",
						i == 0 ? kGreen : (i == path.size() - 1 ? kCyan : ""),
						path[i].c_str(), kReset,
						i < path.size() - 1 ? " ->" : "");
				}
				std::printf("\n");
				continue;
			}

			if (arg.rfind("query ", 0) == 0) {
				// /map query <question> — send graph summary + question to LLM
				auto graph = map::Graph::LoadFromFile(graph_path);
				if (graph.Nodes().empty()) {
					std::printf("%s  no graph found — run /map first%s\n",
						kRed, kReset);
					continue;
				}
				std::string question = arg.substr(6);
				while (!question.empty() && question.front() == ' ')
					question.erase(0, 1);
				// Inject graph summary as context and ask the question.
				std::string prompt = "Here is the knowledge graph of the project:\n\n"
					+ graph.Summary()
					+ "\nAnswer this question about the codebase:\n"
					+ question;
				messages.push_back({"user", {{ContentBlock::TEXT, prompt}}});
				// Fall through to the agent loop (don't continue).
			} else {
				// /map — scan and build graph (with live progress)
				auto progress = [](int cur, int total, const std::string& file) {
					std::fprintf(stderr, "\r\x1b[K  [%d/%d] %s",
						cur, total, file.c_str());
					std::fflush(stderr);
				};
				auto graph = map::ScanWorkspace(root, progress, 50, nullptr);
				std::fprintf(stderr, "\r\x1b[K");  // clear progress line
				std::printf("  %s%zu nodes, %zu edges%s\n", kGreen,
					graph.Nodes().size(), graph.Edges().size(), kReset);
				std::printf("  saved to %s\n", graph_path.c_str());

				// Stamp BFS attributes.
				int stamped = map::StampBfsAttributes(graph);
				std::printf("  BFS attributes stamped on %d files\n", stamped);

				// Generate report via LLM.
				std::string summary = graph.Summary();
				std::string prompt =
					"I just scanned the project and built a knowledge graph.\n\n"
					+ summary
					+ "\nWrite a brief GRAPH_REPORT.md summarizing the architecture: "
					"key modules, most connected components, potential issues, "
					"and suggested areas to explore. Be concise.";
				messages.push_back({"user", {{ContentBlock::TEXT, prompt}}});
				// Fall through to agent loop.
			}
		}

		if (line.rfind("/save", 0) == 0) {
			std::string title_arg;
			if (line.size() > 5) {
				title_arg = line.substr(5);
				while (!title_arg.empty() && title_arg.front() == ' ')
					title_arg.erase(0, 1);
			}
			if (!title_arg.empty()) current_title = title_arg;
			if (current_title.empty()) current_title = "session";
			if (messages.empty()) {
				std::printf("%s(nothing to save)%s\n", kDim, kReset);
				continue;
			}
			std::string err;
			std::string p = SessionStore::save(
				current_session_path, current_title, cfg.model,
				session_input_tokens, session_output_tokens, messages, err);
			if (p.empty()) {
				std::printf("%s[save] %s%s\n", kRed, err.c_str(), kReset);
			} else {
				current_session_path = p;
				std::printf("%s  saved: %s%s\n", kDim, p.c_str(), kReset);
			}
			continue;
		}

		if (line.rfind("/share", 0) == 0) {
			if (messages.empty()) {
				std::printf("%s(nothing to share)%s\n", kDim, kReset);
				continue;
			}
			// Determine output path.
			std::string out_path;
			if (line.size() > 6) {
				out_path = line.substr(6);
				while (!out_path.empty() && out_path.front() == ' ')
					out_path.erase(0, 1);
			}
			if (out_path.empty()) {
				// Generate a filename from the title or timestamp.
				std::string slug = current_title;
				if (slug.empty()) slug = "conversation";
				for (char& c : slug) {
					if (c == ' ' || c == '/') c = '-';
					else c = std::tolower(static_cast<unsigned char>(c));
				}
				out_path = slug + ".md";
			}

			// Export as Markdown.
			std::ofstream out(out_path);
			if (!out) {
				std::printf("%s[share] cannot write %s%s\n",
					kRed, out_path.c_str(), kReset);
				continue;
			}
			out << "# " << (current_title.empty() ? "Conversation" : current_title) << "\n\n";
			out << "*Model: " << cfg.model << " | "
				<< "Tokens: " << session_input_tokens << " in / "
				<< session_output_tokens << " out*\n\n---\n\n";
			for (const auto& m : messages) {
				if (m.role == "user") out << "## You\n\n";
				else if (m.role == "assistant") out << "## Pluto\n\n";
				else continue;
				for (const auto& b : m.content) {
					if (b.type == ContentBlock::TEXT && !b.text.empty())
						out << b.text << "\n\n";
					else if (b.type == ContentBlock::TOOL_USE)
						out << "> **" << b.tool_name << "**\n>\n> ```json\n> "
							<< b.tool_input << "\n> ```\n\n";
					else if (b.type == ContentBlock::TOOL_RESULT)
						out << "> *Result:*\n>\n> ```\n> "
							<< b.text.substr(0, 500) << "\n> ```\n\n";
				}
			}
			out.close();
			std::printf("%s  shared: %s%s\n", kDim, out_path.c_str(), kReset);
			continue;
		}

		if (line == "/list") {
			auto all = SessionStore::list();
			if (all.empty()) {
				std::printf("%s(no saved sessions)%s\n", kDim, kReset);
				continue;
			}
			int idx = 0;
			for (const auto& m : all) {
				if (++idx > 20) { std::printf("  …\n"); break; }
				std::printf("  %s[%2d]%s %s  %s%-24s%s  %lld/%lld tok  %s\n",
					kBold, idx, kReset,
					format_date(m.updated).c_str(),
					kDim, truncate(m.model, 24).c_str(), kReset,
					(long long)m.tokens_in, (long long)m.tokens_out,
					truncate(m.title.empty() ? m.filename : m.title, 60).c_str());
			}
			continue;
		}

		if (line.rfind("/load", 0) == 0) {
			std::string q;
			if (line.size() > 5) {
				q = line.substr(5);
				while (!q.empty() && q.front() == ' ') q.erase(0, 1);
			}
			if (q.empty()) {
				std::printf("  usage: /load <title substring|index>\n");
				continue;
			}
			SessionMetadata meta;
			bool numeric = !q.empty();
			for (char c : q) if (c < '0' || c > '9') { numeric = false; break; }
			if (numeric) {
				auto all = SessionStore::list();
				int idx = std::atoi(q.c_str());
				if (idx >= 1 && (size_t)idx <= all.size())
					meta = all[idx - 1];
			} else {
				meta = SessionStore::find_by_title(q);
			}
			if (meta.path.empty()) {
				std::printf("%s  no session matching '%s'%s\n",
					kRed, q.c_str(), kReset);
				continue;
			}
			std::vector<Message> loaded;
			SessionMetadata loaded_meta;
			std::string err;
			if (!SessionStore::load(meta.path, loaded, loaded_meta, err)) {
				std::printf("%s[load] %s%s\n", kRed, err.c_str(), kReset);
				continue;
			}
			messages = std::move(loaded);
			current_session_path = loaded_meta.path;
			current_title = loaded_meta.title;
			session_input_tokens = loaded_meta.tokens_in;
			session_output_tokens = loaded_meta.tokens_out;
			turns = loaded_meta.turns;
			std::printf("%s  loaded %d turns from %s%s\n",
				kDim, turns, loaded_meta.filename.c_str(), kReset);
			continue;
		}

		if (line == "/resume") {
			SessionMetadata meta = SessionStore::most_recent();
			if (meta.path.empty()) {
				std::printf("%s(no sessions to resume)%s\n", kDim, kReset);
				continue;
			}
			std::vector<Message> loaded;
			SessionMetadata loaded_meta;
			std::string err;
			if (!SessionStore::load(meta.path, loaded, loaded_meta, err)) {
				std::printf("%s[resume] %s%s\n", kRed, err.c_str(), kReset);
				continue;
			}
			messages = std::move(loaded);
			current_session_path = loaded_meta.path;
			current_title = loaded_meta.title;
			session_input_tokens = loaded_meta.tokens_in;
			session_output_tokens = loaded_meta.tokens_out;
			turns = loaded_meta.turns;
			std::printf("%s  resumed: %s (%d turns)%s\n",
				kDim,
				truncate(current_title.empty() ? loaded_meta.filename
				                               : current_title, 60).c_str(),
				turns, kReset);
			continue;
		}

		if (line == "/delete") {
			if (current_session_path.empty()) {
				std::printf("%s(no session loaded)%s\n", kDim, kReset);
				continue;
			}
			std::string err;
			if (!SessionStore::remove(current_session_path, err)) {
				std::printf("%s[delete] %s%s\n", kRed, err.c_str(), kReset);
				continue;
			}
			std::printf("%s  deleted: %s%s\n",
				kDim, current_session_path.c_str(), kReset);
			current_session_path.clear();
			continue;
		}

		if (line.rfind("/title", 0) == 0) {
			if (line.size() <= 7) {
				std::printf("  current: %s\n",
					current_title.empty() ? "(none)" : current_title.c_str());
			} else {
				current_title = line.substr(7);
				while (!current_title.empty() && current_title.front() == ' ')
					current_title.erase(0, 1);
				std::printf("  title: %s\n", current_title.c_str());
			}
			continue;
		}

		if (line.rfind("/tag", 0) == 0 && (line.size() == 4 || line[4] == ' ')) {
			if (current_session_path.empty()) {
				std::printf("%s(no session — /save first)%s\n", kDim, kReset);
				continue;
			}
			std::string tag;
			if (line.size() > 5) {
				tag = line.substr(5);
				while (!tag.empty() && tag.front() == ' ') tag.erase(0, 1);
			}
			if (tag.empty()) {
				auto tags = SessionStore::get_tags(current_session_path);
				if (tags.empty()) std::printf("%s(no tags)%s\n", kDim, kReset);
				else {
					std::printf("  tags: ");
					for (size_t i = 0; i < tags.size(); ++i)
						std::printf("%s%s", i ? ", " : "", tags[i].c_str());
					std::printf("\n");
				}
				continue;
			}
			std::string err;
			if (!SessionStore::add_tag(current_session_path, tag, err))
				std::printf("%s[tag] %s%s\n", kRed, err.c_str(), kReset);
			else
				std::printf("%s  tagged: %s%s\n", kDim, tag.c_str(), kReset);
			continue;
		}

		if (line == "/commands") {
			if (custom_commands.empty()) {
				std::printf("%s(no commands in %s)%s\n",
					kDim, commands_dir().c_str(), kReset);
			} else {
				for (const auto& c : custom_commands) {
					std::string desc = c.description;
					if (desc.empty()) desc = "(no description)";
					std::printf("  %s/%-16s%s  %s\n",
						kBold, c.name.c_str(), kReset, desc.c_str());
				}
			}
			continue;
		}

		// Custom slash command — must be tried BEFORE we fall through to
		// "treat unknown line as user message". If a custom command
		// matches, skip the built-in slash dispatch entirely so the
		// expanded body is sent as a user message, not re-parsed.
		// Custom slash command — must be tried BEFORE we fall through to
		// "treat unknown line as user message". If a custom command
		// matches, skip the built-in slash dispatch entirely so the
		// expanded body is sent as a user message, not re-parsed.
		bool skip_slash_dispatch = false;
		{
			std::string expanded;
			if (try_custom_command(custom_commands, line, expanded)) {
				line = expanded;
				skip_slash_dispatch = true;
			}
		}

		// If a custom command was expanded, skip all built-in slash
		// dispatch — the expanded body goes straight to the user message path.
		if (skip_slash_dispatch) goto pluto_send_message;

		if (line.rfind("/untag ", 0) == 0) {
			if (current_session_path.empty()) {
				std::printf("%s(no session loaded)%s\n", kDim, kReset);
				continue;
			}
			std::string tag = line.substr(7);
			while (!tag.empty() && tag.front() == ' ') tag.erase(0, 1);
			if (tag.empty()) continue;
			std::string err;
			if (!SessionStore::remove_tag(current_session_path, tag, err))
				std::printf("%s[untag] %s%s\n", kRed, err.c_str(), kReset);
			else
				std::printf("%s  untagged: %s%s\n", kDim, tag.c_str(), kReset);
			continue;
		}
		if (line == "/tools") {
			cmd_tools(tools, cfg);
			continue;
		}
		if (line == "/cost") {
			std::printf("  turns:  %d\n", turns);
			std::printf("  input:  %lld tokens\n", session_input_tokens);
			std::printf("  output: %lld tokens\n", session_output_tokens);
			std::printf("  total:  %lld tokens\n",
				session_input_tokens + session_output_tokens);
			continue;
		}
		if (line == "/copy") {
			// Find the last assistant message and concatenate its TEXT blocks.
			std::string text;
			for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
				if (it->role != "assistant") continue;
				for (const auto& b : it->content)
					if (b.type == ContentBlock::TEXT) text += b.text;
				break;
			}
			if (text.empty()) {
				std::printf("%s(nothing to copy)%s\n", kDim, kReset);
				continue;
			}
			BClipboard clipboard("system");
			if (!clipboard.Lock()) {
				std::printf("%s[copy] cannot lock clipboard%s\n", kRed, kReset);
				continue;
			}
			clipboard.Clear();
			BMessage* data = clipboard.Data();
			if (data) {
				data->AddData("text/plain", B_MIME_TYPE,
					text.data(), text.size());
				clipboard.Commit();
			}
			clipboard.Unlock();
			std::printf("%s  copied %zu bytes to clipboard%s\n",
				kDim, text.size(), kReset);
			continue;
		}
		if (line == "/hotkeys") {
			std::printf("REPL key bindings:\n");
			std::printf("  Tab                  complete /slash command or @path\n");
			std::printf("  Up / Down            history previous / next\n");
			std::printf("  Ctrl+R               reverse-search history\n");
			std::printf("  Ctrl+A / Ctrl+E      jump to start / end of line\n");
			std::printf("  Ctrl+W               delete word backward\n");
			std::printf("  Ctrl+U               clear line\n");
			std::printf("  Ctrl+L               clear screen\n");
			std::printf("  Ctrl+C               cancel mid-stream / abort input\n");
			std::printf("  Ctrl+D               EOF (exits on empty line)\n");
			continue;
		}
		if (line.rfind("/models", 0) == 0 && (line.size() == 7 || line[7] == ' ')) {
			// /models [URL] — list models on a remote Ollama/OpenAI server.
			std::string url;
			if (line.size() > 8) {
				url = line.substr(8);
				while (!url.empty() && url.front() == ' ') url.erase(0, 1);
			}
			if (url.empty()) url = cfg.base_url;
			if (url.empty()) url = "http://localhost:11434";

			std::printf("%s  fetching models from %s …%s\n",
				kDim, url.c_str(), kReset);
			std::fflush(stdout);

			std::string err;
			auto models = fetch_remote_models(url, err);
			if (models.empty()) {
				std::printf("%s  %s%s\n", kRed,
					err.empty() ? "no models found" : err.c_str(), kReset);
				continue;
			}
			if (!err.empty())
				std::printf("%s  warning: %s%s\n", kRed, err.c_str(), kReset);

			// Print table.
			std::printf("\n  %s%-30s  %-10s  %-8s  %-8s  %s%s\n",
				kBold, "MODEL", "FAMILY", "PARAMS", "QUANT", "SIZE", kReset);
			for (const auto& m : models) {
				std::printf("  %-30s  %-10s  %-8s  %-8s  %s\n",
					m.name.c_str(),
					m.family.empty() ? "-" : m.family.c_str(),
					m.param_size.empty() ? "-" : m.param_size.c_str(),
					m.quant.empty() ? "-" : m.quant.c_str(),
					human_size(m.size_bytes).c_str());
			}
			std::printf("\n%s  use /model <name> to switch   "
				"(provider will be set to openai-compat)%s\n", kDim, kReset);

			// Switch provider to openai-compat if needed.
			bool need_rebuild = false;
			if (cfg.base_url != url) { cfg.base_url = url; need_rebuild = true; }
			if (cfg.provider != "openai-compat") {
				cfg.provider = "openai-compat";
				need_rebuild = true;
			}
			if (need_rebuild) {
				auto p = CreateProvider(cfg);
				if (p) {
					provider = std::move(p);
					std::printf("%s  provider → openai-compat, base_url → %s%s\n",
						kDim, url.c_str(), kReset);
				}
			}
			if (need_rebuild) SaveConfig(cfg);
			continue;
		}
		if (line.rfind("/model", 0) == 0 && (line.size() == 6 || line[6] == ' ')) {
			if (line.size() <= 7) {
				std::printf("  current: %s\n", cfg.model.c_str());
			} else {
				cfg.model = line.substr(7);
				while (!cfg.model.empty() && cfg.model.front() == ' ')
					cfg.model.erase(0, 1);
				// Auto-detect provider/base_url from model name and
				// recreate the provider if it changed.
				std::string old_prov = cfg.provider;
				std::string old_url  = cfg.base_url;
				AutoDetectProvider(cfg);
				if (cfg.provider != old_prov || cfg.base_url != old_url) {
					auto p = CreateProvider(cfg);
					if (p) {
						provider = std::move(p);
						std::printf("  provider: %s %s\n",
							cfg.provider.c_str(),
							cfg.base_url.empty() ? "" : cfg.base_url.c_str());
					}
				}
				std::printf("  model: %s\n", cfg.model.c_str());
				SaveConfig(cfg);
			}
			continue;
		}
		if (line.rfind("/watch", 0) == 0) {
			if (subscriber && subscriber->running()) {
				std::printf("  already watching %s\n", subscriber->url().c_str());
				continue;
			}
			std::string url = "http://localhost:2607/sse";
			if (line.size() > 7) {
				std::string arg = line.substr(7);
				while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);
				if (!arg.empty()) url = arg;
			}
			subscriber = std::make_unique<SseSubscriber>(url);
			std::string err;
			if (!subscriber->start(err)) {
				std::printf("%s  /watch failed: %s%s\n", kRed, err.c_str(), kReset);
				subscriber.reset();
			} else {
				std::printf("%s  watching %s%s\n", kDim, url.c_str(), kReset);
			}
			continue;
		}
		if (line == "/unwatch") {
			if (!subscriber) {
				std::printf("  not watching\n");
			} else {
				subscriber->stop();
				subscriber.reset();
				std::printf("%s  unwatched%s\n", kDim, kReset);
			}
			continue;
		}
		if (line == "/events") {
			if (!subscriber) {
				std::printf("  not watching (use /watch first)\n");
				continue;
			}
			auto evs = subscriber->drain();
			if (evs.empty()) {
				std::printf("%s  (no pending events)%s\n", kDim, kReset);
			} else {
				for (const auto& e : evs) {
					std::printf("%s  [%s] %s%s\n",
						kDim, e.name.c_str(), e.data.c_str(), kReset);
				}
			}
			continue;
		}
		if (line == "/peers") {
			auto peers = ipc::find_peers();
			if (peers.empty()) {
				std::printf("%s  (no Pluto GUI instances running)%s\n",
					kDim, kReset);
			} else {
				std::printf("\n  %s%-8s  %-20s  %-24s  %-6s  %s%s\n",
					kBold, "PID", "WORKSPACE", "MODEL", "STATE", "SESSION", kReset);
				for (const auto& p : peers) {
					std::string ws = p.workspace;
					if (ws.size() > 20) ws = "..." + ws.substr(ws.size() - 17);
					std::string model = p.model;
					if (model.size() > 24) model = model.substr(0, 21) + "...";
					std::string title = p.session_title;
					if (title.size() > 30) title = title.substr(0, 27) + "...";
					std::printf("  %-8d  %-20s  %-24s  %-6s  %s\n",
						(int)p.team_id,
						ws.empty() ? "?" : ws.c_str(),
						model.empty() ? "?" : model.c_str(),
						p.busy ? "busy" : "idle",
						title.empty() ? "-" : title.c_str());
				}
			}
			std::printf("\n");
			continue;
		}

		// /ask <peer> <prompt> — inject a prompt into a peer's conversation.
		if (line.rfind("/ask ", 0) == 0) {
			std::string args = line.substr(5);
			while (!args.empty() && args.front() == ' ') args.erase(0, 1);
			// Split: first token is peer id (PID or workspace), rest is prompt.
			size_t sp = args.find(' ');
			if (sp == std::string::npos || sp + 1 >= args.size()) {
				std::printf("  usage: /ask <pid|workspace> <prompt>\n");
				continue;
			}
			std::string peer_id = args.substr(0, sp);
			std::string prompt = args.substr(sp + 1);
			while (!prompt.empty() && prompt.front() == ' ') prompt.erase(0, 1);
			if (prompt.empty()) {
				std::printf("  usage: /ask <pid|workspace> <prompt>\n");
				continue;
			}
			auto peer = ipc::find_peer(peer_id);
			if (peer.team_id < 0) {
				std::printf("%s  peer not found: %s%s\n",
					kRed, peer_id.c_str(), kReset);
				std::printf("  use /peers to list running instances\n");
				continue;
			}
			bool ok = ipc::send_to(peer.team_id, ipc::kMsgPeerAsk, prompt);
			if (ok) {
				std::printf("%s  prompt sent to peer %d (%s)%s\n",
					kGreen, (int)peer.team_id,
					peer.workspace.empty() ? "?" : peer.workspace.c_str(),
					kReset);
			} else {
				std::printf("%s  failed to send to peer %d%s\n",
					kRed, (int)peer.team_id, kReset);
			}
			continue;
		}
		// /delegate <peer> <prompt> — fire-and-forget with notification.
		if (line.rfind("/delegate ", 0) == 0) {
			std::string args = line.substr(10);
			while (!args.empty() && args.front() == ' ') args.erase(0, 1);
			size_t sp = args.find(' ');
			if (sp == std::string::npos || sp + 1 >= args.size()) {
				std::printf("  usage: /delegate <pid|workspace> <prompt>\n");
				continue;
			}
			std::string peer_id = args.substr(0, sp);
			std::string prompt = args.substr(sp + 1);
			while (!prompt.empty() && prompt.front() == ' ') prompt.erase(0, 1);
			if (prompt.empty()) {
				std::printf("  usage: /delegate <pid|workspace> <prompt>\n");
				continue;
			}
			auto peer = ipc::find_peer(peer_id);
			if (peer.team_id < 0) {
				std::printf("%s  peer not found: %s%s\n",
					kRed, peer_id.c_str(), kReset);
				std::printf("  use /peers to list running instances\n");
				continue;
			}
			bool ok = ipc::send_to(peer.team_id, ipc::kMsgPeerDelegate, prompt);
			if (ok) {
				std::printf("%s  delegated to peer %d (%s) — will notify when done%s\n",
					kGreen, (int)peer.team_id,
					peer.workspace.empty() ? "?" : peer.workspace.c_str(),
					kReset);
			} else {
				std::printf("%s  failed to send to peer %d%s\n",
					kRed, (int)peer.team_id, kReset);
			}
			continue;
		}

		// /context <peer> <topic> — query a peer for context about a topic.
		if (line.rfind("/context ", 0) == 0) {
			std::string rest = line.substr(9);
			size_t sp = rest.find(' ');
			if (sp == std::string::npos || sp == 0) {
				std::printf("  usage: /context <pid|workspace> <topic>\n");
				continue;
			}
			std::string who = rest.substr(0, sp);
			std::string topic = rest.substr(sp + 1);
			auto peer = ipc::find_peer(who);
			if (peer.team_id < 0) {
				std::printf("%s  peer not found: %s%s\n", kRed, who.c_str(), kReset);
				std::printf("  use /peers to list running instances\n");
				continue;
			}
			std::string ctx = ipc::query_context(peer.team_id, topic);
			if (ctx.empty()) {
				std::printf("%s  (no context found for \"%s\" in peer %d)%s\n",
					kDim, topic.c_str(), (int)peer.team_id, kReset);
			} else {
				std::printf("%s--- context from peer %d ---\n%s%s\n%s--- end ---%s\n",
					kDim, (int)peer.team_id, kReset, ctx.c_str(), kDim, kReset);
			}
			continue;
		}

		if (line == "/routines") {
			auto routines = ListRoutines();
			if (routines.empty()) {
				std::printf("%s(no routines)%s\n", kDim, kReset);
			} else {
				for (const auto& r : routines) {
					std::printf("  %s%-20s%s  [%s]%s\n",
						kBold, r.name.c_str(), kReset,
						r.schedule.c_str(),
						r.enabled ? "" : "  (paused)");
				}
				std::printf("\n  use %s/routine <name>%s to run one in this chat\n",
					kBold, kReset);
			}
			continue;
		}
		if (line.rfind("/routine ", 0) == 0) {
			std::string name = line.substr(9);
			while (!name.empty() && name.front() == ' ') name.erase(0, 1);
			while (!name.empty() && name.back() == ' ') name.pop_back();
			if (name.empty()) {
				std::printf("  usage: /routine <name>\n");
				continue;
			}
			auto routines = ListRoutines();
			bool found = false;
			for (const auto& r : routines) {
				if (r.name == name) {
					line = BuildRoutinePrompt(r);
					found = true;
					break;
				}
			}
			if (!found) {
				std::printf("%s  routine not found: %s%s\n", kRed, name.c_str(), kReset);
				std::printf("  use /routines to see available routines\n");
				continue;
			}
			// Fall through to send the expanded prompt as a user message.
		}

	pluto_send_message:
		// Drain any pending SSE events and prepend them to the user turn as a
		// system-reminder so Claude sees what happened since last turn.
		std::string prelude;
		if (subscriber && subscriber->running()) {
			auto evs = subscriber->drain();
			if (!evs.empty()) {
				prelude = "<system-reminder>\n";
				prelude += "Since your last turn, ";
				prelude += std::to_string(evs.size());
				prelude += " SSE event(s) arrived on ";
				prelude += subscriber->url();
				prelude += ":\n";
				size_t shown = 0;
				for (const auto& e : evs) {
					if (shown++ >= 50) {
						prelude += "... (" + std::to_string(evs.size() - 50) +
							" more suppressed)\n";
						break;
					}
					prelude += "  [";
					prelude += e.name;
					prelude += "] ";
					prelude += e.data;
					prelude += "\n";
				}
				prelude += "</system-reminder>\n\n";

				// Also show them to the user so the injection isn't invisible.
				std::printf("%s  (%zu SSE event(s) injected into context)%s\n",
					kDim, evs.size(), kReset);
			}
		}

		Message user;
		user.role = "user";

		// Build content blocks: text mentions become TEXT, images become IMAGE.
		build_user_content(line, user.content);

		// Prepend SSE prelude to the first text block (or insert one).
		if (!prelude.empty()) {
			bool prepended = false;
			for (auto& b : user.content) {
				if (b.type == ContentBlock::TEXT) {
					b.text = prelude + b.text;
					prepended = true;
					break;
				}
			}
			if (!prepended) {
				ContentBlock tb;
				tb.type = ContentBlock::TEXT;
				tb.text = prelude;
				user.content.insert(user.content.begin(), std::move(tb));
			}
		}
		messages.push_back(std::move(user));

		if (current_title.empty()) current_title = derive_title(line);

		// Hook: user_prompt_submit — fired before the turn.
		// PLUTO_PROMPT env var contains the raw user text.
		// Use shell env-prefix (PLUTO_PROMPT='...' cmd) instead of
		// setenv/unsetenv which are not thread-safe with the SSE
		// background subscriber.
		for (const auto& hook : cfg.hooks_user_prompt) {
			if (hook.empty()) continue;
			pid_t pid = fork();
			if (pid == 0) {
				::setenv("PLUTO_PROMPT", line.c_str(), 1);
				::execl("/bin/sh", "sh", "-c", hook.c_str(), nullptr);
				::_exit(127);
			}
			if (pid > 0) {
				int status = 0;
				::waitpid(pid, &status, 0);
			}
		}

		// Trigger-based skill loading: check if user message matches
		// any triggered skill keywords and inject them.
		auto activated = InjectTriggeredSkills(cfg, line);
		for (const auto& s : activated)
			std::fprintf(stderr, "%s[skill]%s activated: %s\n",
				kDim, kReset, s.c_str());

		ClearCancel();
		md.Reset();
		RunTurn(*provider, cfg, tools, messages, perm, ui);
		// Flush any remaining buffered markdown (partial last line).
		std::string tail = md.Flush();
		if (!tail.empty()) {
			std::printf("%s", kGreen);
			std::fwrite(tail.data(), 1, tail.size(), stdout);
			std::printf("%s", kReset);
		}
		std::printf("\n");

		// Auto-compact when approaching context window limit.
		if (MaybeCompact(*provider, cfg, messages,
		                   (int)session_input_tokens, ui)) {
			std::printf("%s(conversation compacted to free context)%s\n",
				kDim, kReset);
		}
	}
	return 0;
}

}  // namespace pluto
