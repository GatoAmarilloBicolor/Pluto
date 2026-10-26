#include "CliApp.h"
#include "ProviderFactory.h"
#include "ToolRegistryBuilder.h"
#include "McpBootstrap.h"

#include "../agent/Loop.h"
#include "../repl.h"
#include "../oauth.h"
#include "../storage/SessionStore.h"
#include "../rag.h"
#include "../routine.h"
#include "../schedule.h"
#include "../telemetry.h"
#include "../tools/BeapiLookup.h"
#include "../tools/GraphQuery.h"
#include "../tools/Builtin.h"
#include "../tools/Tool.h"
#include "../tools/WebFetch.h"
#include "../tools/WebSearch.h"
#include "../tools/WikiLookup.h"
#include "../util.h"
#include "../wiki.h"
#include "../workspace.h"
#include "../agent_store.h"
#include "../cancel.h"
#include "../json.h"
#include "../config.h"
#include "../mcp/Cli.h"
#include "../mcp/Client.h"
#include "../mcp/Config.h"
#include "../mcp/Manager.h"
#include "../mcp/Transport.h"
#include "../provider/Provider.h"
#include "../provider/Anthropic.h"
#include "../provider/OpenAICompat.h"
#include "../haiku/BfsTools.h"
#include "../haiku/DesktopTools.h"

#include <curl/curl.h>
#include <signal.h>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

void sigint_handler(int) {
    pluto::RequestCancel();
}

void install_sigint() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
}

void usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s [options] [--one-shot PROMPT]\n"
        "       %s -p PROMPT [-f text|json] [-q]\n"
        "  --provider NAME     anthropic (default)\n"
        "  --model ID          model identifier\n"
        "  --base-url URL      api base URL override\n"
        "  --yolo              auto-approve all tool calls\n"
        "  --no-cache          disable ephemeral cache_control on system+tools\n"
        "  --dry-run           print would-be request, don't call the API\n"
        "  --one-shot TEXT     send a single prompt, print response, exit\n"
        "  -p, --prompt TEXT   \n"
        "  -f, --format FMT    text|json\n"
        "  -q, --quiet         suppress tool output\n"
        "  %s mcp <sub>        manage MCP servers (list/get/add/remove/...)\n"
        "  %s agent run <file> run prompt from file as one-shot, auto-approved\n"
        "  %s agent install <p> install agent from path or Git URL\n"
        "  %s agent remove <n> remove installed agent\n"
        "  %s agent list       list installed agents\n"
        "  %s agent info <n>   show agent details\n"
        "  %s agent update <n> update agent from source (or --all)\n"
        "  %s wiki <sub>       manage wiki knowledge base (ingest/search/list/stats)\n"
        "  %s routine <sub>    manage routines (list/run/import/enable/...)\n"
        "  %s schedule <sub>   manage scheduled tasks (list/add/remove/run/...)\n"
        "  config: ~/config/settings/pluto\n"
        "  memory: ~/config/settings/pluto/pluto.md, ./pluto.md\n"
        "  skills: ~/config/settings/pluto/skills/<name>/SKILL.md\n"
        "  ~/config/settings/pluto/commands/<name>.md\n"
        "  MCP: ./.mcp.json, ~/config/settings/pluto/mcp.json,\n"
        "       ~/config/settings/pluto/mcp-local.json\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

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
        if (!quiet) std::fprintf(stderr, "[tool] %s\n", prev.c_str());
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
        using namespace pluto::json;
        Value out(Object{});
        out.set("model", Value(model_used));
        out.set("text", Value(full_text));
        out.set("input_tokens", Value(in_tokens));
        out.set("output_tokens", Value(out_tokens));
        out.set("error", Value(rc));
        std::printf("%s\n", out.serialize().c_str());
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
    ui.on_text = [&](const std::string& t) {
        std::fwrite(t.data(), 1, t.size(), stdout);
        std::fflush(stdout);
    };
    ui.on_tool_start = [](const std::string&, const std::string& prev) {
        std::fprintf(stderr, "[tool] %s\n", prev.c_str());
    };
    ui.on_tool_result = [](const std::string&, const std::string&,
                           const pluto::ToolResult& r) {
        const char* tag = r.is_error ? "[err]" : "[ok]";
        std::string out = r.output;
        if (out.size() > 500) out = out.substr(0, 500) + "...";
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

bool prompt_project_approval(const pluto::McpServerSpec& spec) {
    if (pluto::IsProjectApproved(spec.name)) return true;
    std::printf("\nProject MCP server '%s' requires approval:\n", spec.name.c_str());
    std::printf(" type: %s\n", spec.type.c_str());
    if (!spec.url.empty()) std::printf(" url: %s\n", spec.url.c_str());
    if (!spec.command.empty()) {
        std::printf(" cmd: %s", spec.command.c_str());
        for (const auto& a : spec.args) std::printf(" %s", a.c_str());
        std::printf("\n");
    }
    std::printf(" source: %s\n", spec.source_path.c_str());
    std::printf("Approve? [y]once [a]always [n]o: ");
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
    return true;
}

} // namespace

namespace pluto {

CliApp::CliApp(int argc, char** argv) {
    parse_args(argc, argv);
}

CliApp::~CliApp() = default;

void CliApp::parse_args(int argc, char** argv) {
    if (argc < 2) {
        options_.help = true;
        return;
    }

    // Check for subcommands first
    std::string first_arg = argv[1];
    
    if (first_arg == "trust") {
        trust_command_ = true;
        if (argc >= 3) trust_subcommand_ = argv[2];
        if (argc >= 4) trust_path_ = argv[3];
        return;
    }
    if (first_arg == "mcp") {
        mcp_command_ = true;
        if (argc >= 3) mcp_subcommand_ = argv[2];
        for (int i = 3; i < argc; ++i) mcp_args_.push_back(argv[i]);
        return;
    }
    if (first_arg == "stats") {
        stats_command_ = true;
        return;
    }
    if (first_arg == "routine") {
        routine_command_ = true;
        return;
    }
    if (first_arg == "schedule") {
        schedule_command_ = true;
        return;
    }
    if (first_arg == "wiki") {
        wiki_command_ = true;
        return;
    }
    if (first_arg == "beapi") {
        beapi_command_ = true;
        return;
    }
    if (first_arg == "agent") {
        agent_command_ = true;
        if (argc >= 3) {
            std::string sub = argv[2];
            if (sub == "run" && argc >= 4) {
                agent_run_ = true;
                agent_run_file_ = argv[3];
            }
        }
        return;
    }
    if (first_arg == "login") {
        login_command_ = true;
        return;
    }
    if (first_arg == "logout") {
        logout_command_ = true;
        return;
    }
    if (first_arg == "--help" || first_arg == "-h") {
        options_.help = true;
        return;
    }
    if (first_arg == "--version") {
        options_.version = true;
        return;
    }

    // Parse regular options
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--provider" && i + 1 < argc) { options_.provider = argv[++i]; }
        else if (a == "--model" && i + 1 < argc) { options_.model = argv[++i]; }
        else if (a == "--base-url" && i + 1 < argc) { options_.base_url = argv[++i]; }
        else if (a == "--yolo") { options_.yolo = true; }
        else if (a == "--no-cache") { options_.no_cache = true; }
        else if (a == "--dry-run") { options_.dry_run = true; }
        else if (a == "--one-shot" && i + 1 < argc) { options_.one_shot_prompt = argv[++i]; }
        else if ((a == "-p" || a == "--prompt") && i + 1 < argc) { options_.one_shot_prompt = argv[++i]; }
        else if ((a == "-f" || a == "--format") && i + 1 < argc) { options_.format = argv[++i]; }
        else if (a == "-q" || a == "--quiet") { options_.quiet = true; }
        else if (a == "--help" || a == "-h") { options_.help = true; }
        else if (a == "--version") { options_.version = true; }
        else {
            // Unknown argument, treat as prompt if no one_shot_prompt yet
            if (options_.one_shot_prompt.empty() && a[0] != '-') {
                options_.one_shot_prompt = a;
            }
        }
    }
    
    options_.headless = !options_.one_shot_prompt.empty() || options_.format == "json";
    options_.headless_prompt = options_.one_shot_prompt;
    options_.headless_format = options_.format;
    options_.headless_quiet = options_.quiet;
}

void CliApp::handle_subcommands() {
    if (trust_command_) {
        if (trust_subcommand_ == "list") {
            auto trusted = pluto::ListTrustedWorkspaces();
            if (trusted.empty()) {
                std::printf("(no trusted workspaces)\n");
            } else {
                for (const auto& t : trusted) std::printf("%s\n", t.c_str());
            }
            return;
        }
        if (trust_subcommand_ == "add" && !trust_path_.empty()) {
            pluto::TrustWorkspace(trust_path_);
            std::printf("added: %s\n", trust_path_.c_str());
            return;
        }
        if (trust_subcommand_ == "remove" && !trust_path_.empty()) {
            pluto::UntrustWorkspace(trust_path_);
            std::printf("removed: %s\n", trust_path_.c_str());
            return;
        }
        if (trust_subcommand_ == "log") {
            std::string logpath = pluto::SettingsDir() + "/access.log";
            std::ifstream lf(logpath);
            if (!lf) {
                std::printf("(no access log)\n");
                return;
            }
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(lf, line)) lines.push_back(line);
            size_t start = lines.size() > 50 ? lines.size() - 50 : 0;
            for (size_t i = start; i < lines.size(); ++i)
                std::printf("%s\n", lines[i].c_str());
            return;
        }
        std::fprintf(stderr,
            "usage: %s trust list\n"
            "       %s trust add <path>\n"
            "       %s trust remove <path>\n"
            "       %s trust log\n",
            "pluto", "pluto", "pluto", "pluto");
        return;
    }
    
    if (mcp_command_) {
        std::vector<char*> args;
        args.push_back(const_cast<char*>("pluto"));
        args.push_back(const_cast<char*>("mcp"));
        if (!mcp_subcommand_.empty()) args.push_back(const_cast<char*>(mcp_subcommand_.c_str()));
        for (auto& a : mcp_args_) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        pluto::RunMcpCli(args.size() - 1, args.data());
        return;
    }
    
    if (stats_command_) {
        std::vector<char*> args = {const_cast<char*>("pluto"), const_cast<char*>("stats"), nullptr};
        pluto::RunStatsCli(2, args.data());
        return;
    }
    
    if (routine_command_) {
        std::vector<char*> args = {const_cast<char*>("pluto"), const_cast<char*>("routine"), nullptr};
        pluto::RunRoutineCli(2, args.data());
        return;
    }
    
    if (schedule_command_) {
        std::vector<char*> args = {const_cast<char*>("pluto"), const_cast<char*>("schedule"), nullptr};
        pluto::RunScheduleCli(2, args.data());
        return;
    }
    
    if (wiki_command_) {
        std::vector<char*> args = {const_cast<char*>("pluto"), const_cast<char*>("wiki"), nullptr};
        pluto::RunWikiCli(2, args.data());
        return;
    }
    
    if (beapi_command_) {
        std::vector<char*> args = {const_cast<char*>("pluto"), const_cast<char*>("beapi"), nullptr};
        pluto::RunBeapiCli(2, args.data());
        return;
    }
    
    if (agent_command_) {
        std::vector<char*> args;
        args.push_back(const_cast<char*>("pluto"));
        args.push_back(const_cast<char*>("agent"));
        pluto::RunAgentStoreCli(0, nullptr);
        return;
    }
    
    if (login_command_) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        bool ok = pluto::OAuthLogin();
        if (ok) {
            std::printf("Login successful! Pluto will use Claude account.\n");
            pluto::OAuthTokens tokens = pluto::LoadOAuthTokens();
            if (!tokens.empty()) {
                pluto::OAuthProfile prof = pluto::OAuthGetProfile(tokens.access_token);
                if (!prof.display_name.empty())
                    std::printf(" Logged in as: %s\n", prof.display_name.c_str());
            }
        } else {
            std::fprintf(stderr, "Login failed.\n");
        }
        curl_global_cleanup();
        return;
    }
    
    if (logout_command_) {
        pluto::DeleteOAuthTokens();
        std::printf("Logged out. API key will be used if available.\n");
        return;
    }
}

int CliApp::run() {
    if (options_.help) {
        usage("pluto");
        return 0;
    }
    if (options_.version) {
        std::printf("pluto 0.7.1\n");
        return 0;
    }
    
    handle_subcommands();
    if (trust_command_ || mcp_command_ || stats_command_ || routine_command_ || 
        schedule_command_ || wiki_command_ || beapi_command_ || agent_command_ ||
        login_command_ || logout_command_) {
        return 0;
    }
    
    // Regular CLI operation
    pluto::WorkspaceInit();
    {
        std::string ws = pluto::WorkspaceRoot();
        if (!ws.empty())
            pluto::SessionStore::set_dir_override(ws + "/.pluto/sessions");
    }
    
    cfg_ = pluto::LoadConfig();
    
    // First-run wizard
    if (pluto::IsFirstRun()) {
        if (!pluto::RunFirstRunWizard(cfg_)) return 1;
    }
    
    pluto::AutoDetectProvider(cfg_);
    pluto::ApplyProjectMemory(cfg_);
    pluto::ApplySkills(cfg_);
    pluto::LoadModes(cfg_);
    
    // BeAPI RAG
    std::string rag_path = pluto::RagDefaultPath();
    if (!rag_path.empty()) pluto::RagInit(rag_path);
    if (pluto::RagEntryCount() == 0)
        pluto::RagLoadCustom(pluto::RagCustomPath());
    pluto::ApplyRagHint(cfg_);
    
    // Wiki
    std::string wiki_dir = pluto::WikiDefaultDir();
    int wiki_count = 0;
    if (!wiki_dir.empty()) wiki_count = pluto::WikiInit(wiki_dir);
    
    // Apply CLI options to config
    if (!options_.provider.empty()) cfg_.provider = options_.provider;
    if (!options_.model.empty()) cfg_.model = options_.model;
    if (!options_.base_url.empty()) cfg_.base_url = options_.base_url;
    if (options_.no_cache) cfg_.cache = false;
    cfg_.dry_run = options_.dry_run;
    
    // Resolve credentials
    pluto::ResolveCredentials(cfg_);
    
    // Check for API key
    std::string creds = pluto::ResolveApiKey(cfg_);
    if (creds.empty() && cfg_.provider != "openai-compat") {
        std::fprintf(stderr, "No API key found. Options:\n");
        if (cfg_.provider == "anthropic") {
            std::fprintf(stderr,
                " 1: OAuth (run 'pluto login')\n"
                " 2: export %s=sk-ant-... (API key)\n"
                " 3: use GUI Settings dialog to enter key\n",
                cfg_.api_key_env.c_str());
        } else {
            std::fprintf(stderr,
                " 1: export %s=<key>\n"
                " 2: use GUI Settings dialog to enter key\n"
                " (local servers like Ollama/LM Studio don't need key)\n",
                cfg_.api_key_env.c_str());
        }
        std::fprintf(stderr, "\n");
        return 2;
    }
    
    install_sigint();
    
    // Create provider
    provider_ = ProviderFactory::create(cfg_);
    if (!provider_) return 2;
    
    // Build tool registry
    tools_ = std::make_unique<ToolRegistry>(std::move(ToolRegistryBuilder::build(cfg_)));
    
    // MCP bootstrap
    mcp_manager_ = McpBootstrap::create(*tools_, prompt_project_approval);
    McpBootstrap::auto_discover(*mcp_manager_, *tools_);
    
    // Inject MCP tool descriptions into system prompt
    std::string mcp_block = mcp_manager_->system_prompt_block();
    if (!mcp_block.empty()) {
        if (!cfg_.system_prompt.empty()) cfg_.system_prompt += "\n\n";
        cfg_.system_prompt += mcp_block;
    }
    
    // Run
    int rc = 0;
    if (options_.headless) {
        rc = headless(*provider_, cfg_, *tools_, options_.headless_prompt, options_.headless_format, options_.headless_quiet);
    } else if (!options_.one_shot_prompt.empty()) {
        rc = one_shot(*provider_, cfg_, *tools_, options_.one_shot_prompt);
    } else {
        rc = pluto::RunRepl(provider_, cfg_, *tools_, options_.yolo);
    }
    
    curl_global_cleanup();
    return rc;
}

} // namespace pluto

int main(int argc, char** argv) {
    pluto::CliApp app(argc, argv);
    return app.run();
}
