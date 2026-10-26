// pluto — Haiku-native Claude client.

#include <curl/curl.h>
#include <signal.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "agent/Loop.h"
#include "agent_store.h"
#include "cancel.h"
#include "json.h"
#include "config.h"
#include "haiku/BfsTools.h"
#include "haiku/DesktopTools.h"
#include "haiku/PluginLoader.h"
#include "systemone/SystemOneClient.h"
#include "mcp/Cli.h"
#include "mcp/Client.h"
#include "mcp/Config.h"
#include "mcp/Manager.h"
#include "mcp/Transport.h"
#include "mcp/builtin/builtin_servers.h"
#include "mcp/builtin/mcp_builtin_manager.h"
// Provider headers no longer needed here — CreateProvider() in config.cpp
// handles instantiation.
#include "provider/Provider.h"
#include "repl.h"
#include "oauth.h"
#include "storage/SessionStore.h"
#include "rag.h"
#include "routine.h"
#include "schedule.h"
#include "telemetry.h"
#include "tools/BeapiLookup.h"
#include "tools/GraphQuery.h"
#include "tools/Builtin.h"
#include "tools/Tool.h"
#include "tools/WebFetch.h"
#include "tools/WebSearch.h"
#include "tools/WikiLookup.h"
#include "util.h"
#include "wiki.h"
#include "workspace.h"

namespace {

void sigint_handler(int) {
	pluto::RequestCancel();
}

void install_sigint() {
	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;   // restart interrupted syscalls (e.g. fgets)
	sigaction(SIGINT, &sa, nullptr);
}

void usage(const char* prog) {
	std::fprintf(stderr,
		"usage: %s [options] [--one-shot PROMPT]\n"
		"       %s -p PROMPT [-f text|json] [-q]\n"
		"  --provider NAME   anthropic (default)\n"
		"  --model ID        model identifier\n"
		"  --base-url URL    api base URL override\n"
		"  --yolo            auto-approve all tool calls\n"
		"  --no-cache        disable ephemeral cache_control on system+tools\n"
		"  --dry-run         print would-be request, don't call the API\n"
		"  --one-shot TEXT   send a single prompt, print response, exit\n"
		"  -p, --prompt TEXT headless mode: single turn, print to stdout, exit\n"
		"  -f, --format FMT  output format: text (default) or json\n"
		"  -q, --quiet       suppress status messages on stderr\n"
		"\n"
		"  %s mcp <sub>           manage MCP servers (list/get/add/remove/...)\n"
		"  %s agent run <file>    run prompt from file as one-shot, auto-approved\n"
		"  %s agent install <p>  install agent from path or Git URL\n"
		"  %s agent remove <n>   remove installed agent\n"
		"  %s agent list         list installed agents\n"
		"  %s agent info <n>     show agent details\n"
		"  %s agent update <n>   update agent from source (or --all)\n"
		"  %s wiki <sub>         manage wiki knowledge base (ingest/search/list/stats)\n"
		"  %s routine <sub>      manage routines (list/run/import/enable/...)\n"
		"  %s schedule <sub>     manage scheduled tasks (list/add/remove/run/...)\n"
		"\n"
		"  config:   ~/config/settings/pluto\n"
		"  memory:   ~/config/settings/pluto/pluto.md, ./pluto.md\n"
		"  skills:   ~/config/settings/pluto/skills/<name>/SKILL.md\n"
		"  commands: ~/config/settings/pluto/commands/<name>.md\n"
		"  MCP:      ./.mcp.json, ~/config/settings/pluto/mcp.json,\n"
		"            ~/config/settings/pluto/mcp-local.json\n",
		prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

// Headless mode: single turn, structured output, suitable for scripting.
int headless(pluto::Provider& p, const pluto::Config& cfg,
             pluto::ToolRegistry& tools, const std::string& prompt,
             const std::string& format, bool quiet) {
	std::vector<pluto::Message> messages;
	pluto::Message user;
	user.role = "user";
	pluto::ContentBlock tb;
	tb.type = pluto::ContentBlock::TEXT;
	tb.text = prompt;
	user.content.push_back(std::move(tb));
	messages.push_back(std::move(user));

	int rc = 0;
	std::string full_text;
	int in_tokens = 0, out_tokens = 0;
	std::string model_used = cfg.model;

	pluto::UiCallbacks ui;
	ui.on_text = [&](const std::string& t) {
		full_text += t;
		if (format != "json") {
			std::fwrite(t.data(), 1, t.size(), stdout);
			std::fflush(stdout);
		}
	};
	ui.on_tool_start = [&quiet](const std::string&, const std::string& prev) {
		if (!quiet)
			std::fprintf(stderr, "[tool] %s\n", prev.c_str());
	};
	ui.on_tool_result = [&quiet](const std::string&, const std::string&,
	                             const pluto::ToolResult& r) {
		if (!quiet) {
			const char* tag = r.is_error ? "[err]" : "[ok]";
			std::string out = r.output;
			if (out.size() > 500) out = out.substr(0, 500) + "...";
			std::fprintf(stderr, "%s %s\n", tag, out.c_str());
		}
	};
	ui.on_error = [&rc, &quiet](const std::string& e) {
		if (!quiet) std::fprintf(stderr, "[error] %s\n", e.c_str());
		rc = 1;
	};
	ui.on_usage = [&](int in_t, int out_t, int, int) {
		in_tokens += in_t;
		out_tokens += out_t;
	};

	pluto::RunTurn(p, cfg, tools, messages, pluto::AutoAllow(), ui);

	if (format == "json") {
		// Structured JSON output for scripting.
		pluto::json::Value out;
		out.set("model", model_used);
		out.set("text", full_text);
		out.set("input_tokens", (double)in_tokens);
		out.set("output_tokens", (double)out_tokens);
		out.set("error", rc != 0);
		// Collect tool calls from the conversation.
		pluto::json::Value tool_calls;
		for (const auto& m : messages) {
			if (m.role != "assistant") continue;
			for (const auto& b : m.content) {
				if (b.type == pluto::ContentBlock::TOOL_USE) {
					pluto::json::Value tc;
					tc.set("name", b.tool_name);
					tc.set("input", b.tool_input);
					tool_calls.push(std::move(tc));
				}
			}
		}
		out.set("tool_calls", std::move(tool_calls));
		std::printf("%s\n", out.serialize().c_str());
	} else {
		std::printf("\n");
	}
	return rc;
}

int one_shot(pluto::Provider& p, const pluto::Config& cfg,
             pluto::ToolRegistry& tools, const std::string& prompt) {
	std::vector<pluto::Message> messages;
	pluto::Message user;
	user.role = "user";
	pluto::ContentBlock tb;
	tb.type = pluto::ContentBlock::TEXT;
	tb.text = prompt;
	user.content.push_back(std::move(tb));
	messages.push_back(std::move(user));

	int rc = 0;
	pluto::UiCallbacks ui;
	ui.on_text = [](const std::string& t) {
		std::fwrite(t.data(), 1, t.size(), stdout);
		std::fflush(stdout);
	};
	ui.on_tool_start = [](const std::string&, const std::string& prev) {
		std::fprintf(stderr, "\n[tool] %s\n", prev.c_str());
	};
	ui.on_tool_result = [](const std::string&, const std::string&,
	                       const pluto::ToolResult& r) {
		const char* tag = r.is_error ? "[err]" : "[ok]";
		std::string out = r.output;
		if (out.size() > 500) out = out.substr(0, 500) + "... [truncated]";
		std::fprintf(stderr, "%s %s\n", tag, out.c_str());
	};
	ui.on_error = [&rc](const std::string& e) {
		std::fprintf(stderr, "[error] %s\n", e.c_str());
		rc = 1;
	};

	pluto::RunTurn(p, cfg, tools, messages, pluto::AutoAllow(), ui);
	std::printf("\n");
	return rc;
}

// Returns true if the project-scoped server should be loaded.
bool prompt_project_approval(const pluto::McpServerSpec& spec) {
	if (pluto::IsProjectApproved(spec.name)) return true;

	std::printf("\nProject MCP server '%s' requires approval:\n", spec.name.c_str());
	std::printf("  type:   %s\n", spec.type.c_str());
	if (!spec.url.empty())     std::printf("  url:    %s\n", spec.url.c_str());
	if (!spec.command.empty()) {
		std::printf("  cmd:    %s", spec.command.c_str());
		for (const auto& a : spec.args) std::printf(" %s", a.c_str());
		std::printf("\n");
	}
	std::printf("  source: %s\n", spec.source_path.c_str());
	std::printf("Approve?  [y]once  [a]always  [n]o: ");
	std::fflush(stdout);

	char buf[32];
	if (!std::fgets(buf, sizeof(buf), stdin)) return false;
	char c = 0;
	for (char* p = buf; *p; ++p) {
		if (!std::isspace(static_cast<unsigned char>(*p))) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
			break;
		}
	}
	if (c == 'n') return false;
	if (c == 'a') pluto::SetProjectApproved(spec.name);
	return true;  // y or default
}

}  // namespace

int main(int argc, char** argv) {
	pluto::WorkspaceInit();

	// Use project-local session storage when inside a workspace.
	{
		std::string ws = pluto::WorkspaceRoot();
		if (!ws.empty()) {
			pluto::SessionStore::set_dir_override(
				ws + "/.pluto/sessions");
		}
	}

	// Subcommand dispatch: `pluto mcp ...` bypasses the chat UI entirely.
	if (argc >= 2 && std::string(argv[1]) == "mcp") {
		return pluto::RunMcpCli(argc - 2, argv + 2);
	}

	// Subcommand: pluto stats [reset]
	if (argc >= 2 && std::string(argv[1]) == "stats") {
		return pluto::RunStatsCli(argc - 2, argv + 2);
	}

	// Subcommand: pluto routine ...
	if (argc >= 2 && std::string(argv[1]) == "routine") {
		return pluto::RunRoutineCli(argc - 2, argv + 2);
	}

	// Subcommand: pluto schedule ...
	if (argc >= 2 && std::string(argv[1]) == "schedule") {
		return pluto::RunScheduleCli(argc - 2, argv + 2);
	}

	// Subcommand: pluto trust ...
	if (argc >= 2 && std::string(argv[1]) == "trust") {
		std::string sub = (argc >= 3) ? argv[2] : "";
		if (sub == "list") {
			auto trusted = pluto::ListTrustedWorkspaces();
			if (trusted.empty()) {
				std::printf("(no trusted workspaces)\n");
			} else {
				for (const auto& t : trusted)
					std::printf("  %s\n", t.c_str());
			}
			return 0;
		}
		if (sub == "add" && argc >= 4) {
			std::string path = argv[3];
			pluto::TrustWorkspace(path);
			std::printf("trusted: %s\n", path.c_str());
			return 0;
		}
		if (sub == "remove" && argc >= 4) {
			std::string path = argv[3];
			pluto::UntrustWorkspace(path);
			std::printf("removed: %s\n", path.c_str());
			return 0;
		}
		if (sub == "log") {
			// Print last 50 lines of access.log
			std::string logpath = pluto::SettingsDir() + "/access.log";
			std::ifstream lf(logpath);
			if (!lf) {
				std::printf("(no access log)\n");
				return 0;
			}
			std::vector<std::string> lines;
			std::string line;
			while (std::getline(lf, line)) lines.push_back(line);
			size_t start = lines.size() > 50 ? lines.size() - 50 : 0;
			for (size_t i = start; i < lines.size(); ++i)
				std::printf("%s\n", lines[i].c_str());
			return 0;
		}
		std::fprintf(stderr,
			"usage: %s trust list\n"
			"       %s trust add <path>\n"
			"       %s trust remove <path>\n"
			"       %s trust log\n",
			argv[0], argv[0], argv[0], argv[0]);
		return 2;
	}

	// Subcommand: pluto login
	if (argc >= 2 && std::string(argv[1]) == "login") {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		bool ok = pluto::OAuthLogin();
		if (ok) {
			std::printf("Login successful! Pluto will use your Claude account.\n");
			pluto::OAuthTokens tokens = pluto::LoadOAuthTokens();
			if (!tokens.empty()) {
				pluto::OAuthProfile prof = pluto::OAuthGetProfile(
					tokens.access_token);
				if (!prof.display_name.empty())
					std::printf("  Logged in as: %s\n",
						prof.display_name.c_str());
			}
		} else {
			std::fprintf(stderr, "Login failed.\n");
		}
		curl_global_cleanup();
		return ok ? 0 : 1;
	}

	// Subcommand: pluto logout
	if (argc >= 2 && std::string(argv[1]) == "logout") {
		pluto::DeleteOAuthTokens();
		std::printf("Logged out. Pluto will use API key if set.\n");
		return 0;
	}

	// Subcommand: pluto wiki <sub>
	if (argc >= 2 && std::string(argv[1]) == "wiki") {
		return pluto::RunWikiCli(argc - 2, argv + 2);
	}

	// Subcommand: pluto beapi <query>
	if (argc >= 2 && std::string(argv[1]) == "beapi") {
		return pluto::RunBeapiCli(argc - 2, argv + 2);
	}

	pluto::Config cfg = pluto::LoadConfig();

	// First-run wizard: if no config file exists, help the user pick a provider.
	if (pluto::IsFirstRun()) {
		pluto::RunFirstRunWizard(cfg);
	}

	pluto::AutoDetectProvider(cfg);
	pluto::ApplyProjectMemory(cfg);
	pluto::ApplySkills(cfg);
	pluto::LoadModes(cfg);

	// Load BeAPI RAG index for Haiku-native code assistance.
	std::string rag_path = pluto::RagDefaultPath();
	if (!rag_path.empty()) {
		int n = pluto::RagInit(rag_path);
		if (n > 0) {
			pluto::RagLoadCustom(pluto::RagCustomPath());
			pluto::ApplyRagHint(cfg);
		}
	}

	// Load wiki knowledge base (markdown files).
	std::string wiki_dir = pluto::WikiDefaultDir();
	int wiki_count = 0;
	if (!wiki_dir.empty()) {
		wiki_count = pluto::WikiInit(wiki_dir);
	}

	std::string one_shot_prompt;
	std::string headless_prompt;
	std::string headless_format = "text";
	bool headless_quiet = false;
	bool yolo = false;
	int args_start = 1;

	// Subcommand: pluto agent install/remove/list/info
	if (argc >= 3 && std::string(argv[1]) == "agent") {
		std::string sub = argv[2];
		if (sub == "install" || sub == "remove" || sub == "list"
		    || sub == "info" || sub == "update" || sub == "search") {
			return pluto::RunAgentStoreCli(argc - 2, argv + 2);
		}
	}

	// Subcommand: pluto agent run <file.md> — read file, run as one-shot,
	// auto-approve tool calls. Remaining args are still parsed normally.
	if (argc >= 4 && std::string(argv[1]) == "agent"
	              && std::string(argv[2]) == "run") {
		// Verify workspace trust before auto-approving all tool calls.
		std::string ws = pluto::WorkspaceRoot();
		if (!ws.empty() && !pluto::IsWorkspaceTrusted(ws)
		    && !pluto::IsSessionTrusted()) {
			std::fprintf(stderr,
				"error: workspace '%s' is not trusted.\n"
				"  Use 'pluto trust add %s' first, or run interactively.\n",
				ws.c_str(), ws.c_str());
			return 2;
		}
		std::ifstream f(argv[3]);
		if (!f) {
			std::fprintf(stderr, "cannot open: %s\n", argv[3]);
			return 2;
		}
		std::ostringstream os;
		os << f.rdbuf();
		one_shot_prompt = os.str();
		yolo = true;
		args_start = 4;
	}

	for (int i = args_start; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
		else if (a == "--provider" && i + 1 < argc) cfg.provider = argv[++i];
		else if (a == "--model"    && i + 1 < argc) cfg.model    = argv[++i];
		else if (a == "--base-url" && i + 1 < argc) cfg.base_url = argv[++i];
		else if (a == "--one-shot" && i + 1 < argc) one_shot_prompt = argv[++i];
		else if ((a == "-p" || a == "--prompt") && i + 1 < argc) headless_prompt = argv[++i];
		else if ((a == "-f" || a == "--format") && i + 1 < argc) headless_format = argv[++i];
		else if (a == "-q" || a == "--quiet") headless_quiet = true;
		else if (a == "--yolo") yolo = true;
		else if (a == "--no-cache") cfg.cache = false;
		else if (a == "--dry-run") cfg.dry_run = true;
		else { usage(argv[0]); return 2; }
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	auto creds = pluto::ResolveCredentials(cfg);
	if (creds.empty()) {
		std::fprintf(stderr,
			"No credentials found for provider '%s'.\n\n",
			cfg.provider.c_str());
		if (cfg.provider == "anthropic") {
			std::fprintf(stderr,
				"  Option 1: pluto login                (Claude Max/Pro via OAuth)\n"
				"  Option 2: export %s=sk-ant-...   (API key via env)\n"
				"  Option 3: use the GUI Settings dialog to enter a key\n",
				cfg.api_key_env.c_str());
		} else {
			std::fprintf(stderr,
				"  Option 1: export %s=<key>\n"
				"  Option 2: use the GUI Settings dialog to enter a key\n"
				"  (local servers like Ollama/LM Studio don't need a key)\n",
				cfg.api_key_env.c_str());
		}
		std::fprintf(stderr, "\n");
		curl_global_cleanup();
		return 2;
	}

	install_sigint();

	auto provider = pluto::CreateProvider(cfg);
	if (!provider) { curl_global_cleanup(); return 2; }

	pluto::ToolRegistry tools;
	tools.add(pluto::MakeReadFileTool());
	tools.add(pluto::MakeWriteFileTool());
	tools.add(pluto::MakeEditFileTool());
	tools.add(pluto::MakeBashTool());
	tools.add(pluto::MakeGrepTool());
	tools.add(pluto::MakeGlobTool());
	tools.add(pluto::MakeReadAttrTool());
	tools.add(pluto::MakeWriteAttrTool());
	tools.add(pluto::MakeListAttrsTool());
	tools.add(pluto::MakeRemoveAttrTool());
	tools.add(pluto::MakeQueryFsTool());
	tools.add(pluto::MakeDesktopScriptTool());
	tools.add(pluto::MakeLaunchAppTool());
	tools.add(pluto::MakeLiveQueryTool());
	tools.add(pluto::MakeClipboardTool());
	tools.add(pluto::MakeNodeMonitorTool());
	if (pluto::RagEntryCount() > 0)
		tools.add(pluto::MakeBeapiLookupTool());
	if (wiki_count > 0)
		tools.add(pluto::MakeWikiLookupTool());
	tools.add(pluto::MakeWebFetchTool());
	tools.add(pluto::MakeWebSearchTool());
	tools.add(pluto::MakeTodoWriteTool());
	tools.add(pluto::MakeApplyPatchTool());
	tools.add(pluto::MakeLspTool());
	tools.add(pluto::MakeGraphQueryTool());

	// Load Haiku-native add-ons from the plugins directory.
#ifdef __HAIKU__
	std::string plugins_dir = pluto::SettingsDir() + "/plugins";
	pluto::LoadAndRegisterPlugins(plugins_dir, tools);
#endif

	// ---- System One Decision Engine ----
	// Load System One config and register decision tools if enabled.
	pluto::systemone::SystemOneConfig sys1_cfg = pluto::systemone::LoadSystemOneConfig();
	// Override with config from main Config if available
	sys1_cfg.enabled = cfg.systemone_enabled;
	sys1_cfg.model = pluto::systemone::DecisionModelTypeFromString(cfg.systemone_model);
	sys1_cfg.api_key = cfg.systemone_api_key;
	sys1_cfg.base_url = cfg.systemone_base_url;
	sys1_cfg.kev_url = cfg.systemone_kev_url;
	sys1_cfg.laya_url = cfg.systemone_laya_url;
	sys1_cfg.jev_url = cfg.systemone_jev_url;
	sys1_cfg.default_threshold = cfg.systemone_threshold;
	sys1_cfg.timeout_secs = cfg.systemone_timeout_secs;
	sys1_cfg.prefer_local = cfg.systemone_prefer_local;
	sys1_cfg.fallback_to_local = cfg.systemone_fallback_to_local;

	std::unique_ptr<pluto::systemone::SystemOneClient> systemone_client;
	if (pluto::IsSystemOneEnabled()) {
		systemone_client = pluto::systemone::CreateSystemOneClient(sys1_cfg, tools);
		if (systemone_client) {
			fprintf(stderr, "[systemone] enabled with model: %s\n",
				systemone_client->model_name().c_str());
		}
	}

	// MCP bootstrap.
	pluto::McpManager mcp(tools, prompt_project_approval);

	// Auto-discover local MCP servers not already configured.
	for (const auto& d : pluto::DiscoverMcpServers()) {
		pluto::McpServerSpec spec;
		spec.name = d.name;
		spec.type = d.type;
		spec.url  = d.url;
		spec.scope = pluto::McpServerSpec::LOCAL;
		std::string err;
		auto transport = pluto::MakeTransport(spec, err);
		if (!transport || !transport->start(err)) continue;
		auto client = std::make_shared<pluto::McpClient>(
			spec.name, std::move(transport));
		if (!client->connect()) continue;
		std::fprintf(stderr, "[discover/%s] %zu tools\n",
			spec.name.c_str(), client->tools().size());
		mcp.add_client(client, tools);
	}

	// Inject MCP tool descriptions into the system prompt so the model
	// knows what remote tools are available and when to use them.
	{
		std::string mcp_block = mcp.system_prompt_block();
		if (!mcp_block.empty()) {
			if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
			cfg.system_prompt += mcp_block;
		}
	}

	// ---- Built-in MCP servers (Serena, Context7, DeepWiki, REA, Memory,
	// Playwright, SQLite) ----
	// Launch stdio servers and register their tools into the same registry.
	pluto::mcp::BuiltinMcpManager builtin_mcp;
	int builtin_count = builtin_mcp.init(tools);
	if (builtin_count > 0) {
		std::string builtin_block = builtin_mcp.system_prompt_block();
		if (!builtin_block.empty()) {
			if (!cfg.system_prompt.empty()) cfg.system_prompt += "\n\n";
			cfg.system_prompt += builtin_block;
		}
	}

	int rc;
	if (!headless_prompt.empty())
		rc = headless(*provider, cfg, tools, headless_prompt,
		              headless_format, headless_quiet);
	else if (!one_shot_prompt.empty())
		rc = one_shot(*provider, cfg, tools, one_shot_prompt);
	else
		rc = pluto::RunRepl(provider, cfg, tools, yolo);

	curl_global_cleanup();
	return rc;
}
