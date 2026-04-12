// pluto — Haiku-native Claude client.

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "agent/Loop.h"
#include "config.h"
#include "mcp/Cli.h"
#include "mcp/Client.h"
#include "mcp/Config.h"
#include "mcp/Transport.h"
#include "provider/Anthropic.h"
#include "provider/OpenAICompat.h"
#include "provider/Provider.h"
#include "repl.h"
#include "tools/Builtin.h"
#include "tools/Tool.h"

namespace {

void usage(const char* prog) {
	std::fprintf(stderr,
		"usage: %s [options] [--one-shot PROMPT]\n"
		"  --provider NAME   anthropic (default) | openai-compat (stub)\n"
		"  --model ID        model identifier\n"
		"  --base-url URL    openai-compat base URL\n"
		"  --yolo            auto-approve all tool calls\n"
		"  --one-shot TEXT   send a single prompt, print response, exit\n"
		"\n"
		"  %s mcp <sub>      manage MCP servers (list/get/add/remove/...)\n"
		"\n"
		"  config:   ~/config/settings/pluto\n"
		"  MCP:      ./.mcp.json (project), ~/config/settings/pluto/mcp.json (user),\n"
		"            ~/config/settings/pluto/mcp-local.json (local, per-cwd)\n",
		prog, prog);
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
	ui.on_tool_result = [](const std::string&, const pluto::ToolResult& r) {
		const char* tag = r.is_error ? "[err]" : "[ok]";
		std::string out = r.output;
		if (out.size() > 500) out = out.substr(0, 500) + "... [truncated]";
		std::fprintf(stderr, "%s %s\n", tag, out.c_str());
	};
	ui.on_error = [&rc](const std::string& e) {
		std::fprintf(stderr, "[error] %s\n", e.c_str());
		rc = 1;
	};

	pluto::run_turn(p, cfg, tools, messages, pluto::auto_allow(), ui);
	std::printf("\n");
	return rc;
}

std::unique_ptr<pluto::Provider> make_provider(const pluto::Config& cfg,
                                                const std::string& key) {
	if (cfg.provider == "anthropic")
		return std::make_unique<pluto::AnthropicProvider>(key);
	if (cfg.provider == "openai-compat") {
		if (cfg.base_url.empty()) {
			std::fprintf(stderr, "openai-compat requires base_url\n");
			return nullptr;
		}
		return std::make_unique<pluto::OpenAICompatProvider>(key, cfg.base_url);
	}
	std::fprintf(stderr, "unknown provider: %s\n", cfg.provider.c_str());
	return nullptr;
}

// Returns true if the project-scoped server should be loaded.
bool prompt_project_approval(const pluto::McpServerSpec& spec) {
	if (pluto::is_project_approved(spec.name)) return true;

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
	if (c == 'a') pluto::set_project_approved(spec.name);
	return true;  // y or default
}

}  // namespace

int main(int argc, char** argv) {
	// Subcommand dispatch: `pluto mcp ...` bypasses the chat UI entirely.
	if (argc >= 2 && std::string(argv[1]) == "mcp") {
		return pluto::run_mcp_cli(argc - 2, argv + 2);
	}

	pluto::Config cfg = pluto::load_config();
	std::string one_shot_prompt;
	bool yolo = false;

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
		else if (a == "--provider" && i + 1 < argc) cfg.provider = argv[++i];
		else if (a == "--model"    && i + 1 < argc) cfg.model    = argv[++i];
		else if (a == "--base-url" && i + 1 < argc) cfg.base_url = argv[++i];
		else if (a == "--one-shot" && i + 1 < argc) one_shot_prompt = argv[++i];
		else if (a == "--yolo") yolo = true;
		else { usage(argv[0]); return 2; }
	}

	std::string api_key = pluto::resolve_api_key(cfg);
	if (api_key.empty()) {
		std::fprintf(stderr, "%s not set\n", cfg.api_key_env.c_str());
		return 2;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	auto provider = make_provider(cfg, api_key);
	if (!provider) { curl_global_cleanup(); return 2; }

	pluto::ToolRegistry tools;
	tools.add(pluto::make_read_file_tool());
	tools.add(pluto::make_write_file_tool());
	tools.add(pluto::make_edit_file_tool());
	tools.add(pluto::make_bash_tool());
	tools.add(pluto::make_grep_tool());
	tools.add(pluto::make_glob_tool());

	// MCP servers from .mcp.json + user + local scopes. Clients must outlive
	// the registry since RemoteTool holds raw ptrs.
	std::vector<std::unique_ptr<pluto::McpClient>> mcp_clients;
	for (auto& spec : pluto::load_mcp_specs()) {
		if (spec.scope == pluto::McpServerSpec::PROJECT &&
		    !prompt_project_approval(spec)) {
			std::fprintf(stderr, "[mcp/%s] denied\n", spec.name.c_str());
			continue;
		}

		std::string err;
		auto transport = pluto::make_transport(spec, err);
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
		auto client = std::make_unique<pluto::McpClient>(spec.name, std::move(transport));
		if (!client->connect()) continue;
		std::fprintf(stderr, "[mcp/%s] %zu tools\n",
			spec.name.c_str(), client->tools().size());
		pluto::register_mcp_tools(*client, tools);
		mcp_clients.push_back(std::move(client));
	}

	int rc = one_shot_prompt.empty()
		? pluto::run_repl(*provider, cfg, tools, yolo)
		: one_shot(*provider, cfg, tools, one_shot_prompt);

	curl_global_cleanup();
	return rc;
}
