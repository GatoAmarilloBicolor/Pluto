#include "App.h"

#include <Alert.h>
#include <Message.h>
#include <PropertyInfo.h>
#include <Roster.h>
#include <String.h>

#include <curl/curl.h>

#include <cstdio>
#include <unistd.h>

#include "MainWindow.h"

#include "../ipc/PlutoMessenger.h"
#include "../mcp/Client.h"
#include "../mcp/Config.h"
#include "../mcp/Manager.h"
#include "../mcp/Transport.h"
#include "../oauth.h"
#include "../provider/Anthropic.h"
#include "../provider/OpenAICompat.h"
#include "../storage/SessionStore.h"
#include "../workspace.h"

namespace pluto {
namespace gui {

namespace {

// Build a unique app signature. If the base signature is already running,
// append PID so multiple instances can coexist.
const char* kBaseSignature = "application/x-vnd.Hay-pluto";
static char sSignature[64];

const char* pick_signature() {
	if (!be_roster->IsRunning(kBaseSignature)) {
		return kBaseSignature;
	}
	snprintf(sSignature, sizeof(sSignature),
		"application/x-vnd.Hay-pluto-%d", (int)getpid());
	return sSignature;
}

}  // namespace

App::App(Config* cfg, ToolRegistry* tools, std::string initial_key)
	: BApplication(pick_signature()),
	  fConfig(cfg), fTools(tools), fApiKey(std::move(initial_key)) {}

App::~App() = default;

void App::ReadyToRun() {
	// Tab forwarding: if the base signature is already running (i.e. we
	// are a secondary instance), try to forward to the first instance
	// as a new tab instead of opening a new window.
	if (strcmp(Signature(), kBaseSignature) != 0) {
		// We're a secondary instance. Try to forward to the primary.
		BMessenger primary(kBaseSignature);
		if (primary.IsValid()) {
			BMessage fwd('plOT');  // kMsgOpenNewTab
			BMessage reply;
			status_t err = primary.SendMessage(&fwd, &reply, 2000000, 2000000);
			if (err == B_OK) {
				int32 code = B_ERROR;
				reply.FindInt32("error", &code);
				if (code == B_OK) {
					// Forwarding succeeded — quit this instance.
					PostMessage(B_QUIT_REQUESTED);
					return;
				}
			}
		}
		// If forwarding failed, fall through and open normally.
	}
	_LaunchMain();
}

void App::RefsReceived(BMessage* msg) {
	// If the message contains ask_pluto_cwd (from Tracker add-on),
	// switch the workspace immediately — even before MainWindow exists.
	// This ensures the correct workspace is set for the trust prompt
	// and project config loading in _LaunchMain().
	{
		const char* cwd = nullptr;
		if (msg->FindString("ask_pluto_cwd", &cwd) == B_OK
		    && cwd && *cwd) {
			std::string cur = pluto::WorkspaceRoot();
			std::string next(cwd);
			if (next != cur) {
				pluto::WorkspaceSetRoot(next);
				pluto::SessionStore::set_dir_override(
					next + "/.pluto/sessions");
				std::fprintf(stderr, "[pluto/app] workspace → %s\n", cwd);
			}
		}
	}

	// Forward to MainWindow if it exists.
	for (int32 i = 0; i < CountWindows(); ++i) {
		BWindow* win = WindowAt(i);
		if (dynamic_cast<MainWindow*>(win)) {
			BMessenger(win).SendMessage(msg);
			return;
		}
	}
	// MainWindow not yet created — stash for later delivery.
	fPendingRefs = *msg;
	fHasPendingRefs = true;
}

void App::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case B_GET_PROPERTY: {
			int32 index;
			BMessage specifier;
			int32 what_spec;
			const char* property;
			if (msg->GetCurrentSpecifier(&index, &specifier, &what_spec, &property) != B_OK) {
				BApplication::MessageReceived(msg);
				break;
			}

			BMessage reply(B_REPLY);
			if (strcmp(property, "Workspace") == 0) {
				reply.AddString("result", pluto::WorkspaceRoot().c_str());
				reply.AddInt32("error", B_OK);
			} else if (strcmp(property, "Model") == 0) {
				MainWindow* mw = _FindMainWindow();
				std::string mdl = mw ? mw->GetActiveModel()
				                     : (fConfig ? fConfig->model : "");
				reply.AddString("result", mdl.c_str());
				reply.AddInt32("error", B_OK);
			} else if (strcmp(property, "SessionTitle") == 0) {
				MainWindow* mw = _FindMainWindow();
				reply.AddString("result",
					mw ? mw->GetSessionTitle().c_str() : "");
				reply.AddInt32("error", B_OK);
			} else if (strcmp(property, "Busy") == 0) {
				MainWindow* mw = _FindMainWindow();
				reply.AddBool("result", mw ? mw->IsBusy() : false);
				reply.AddInt32("error", B_OK);
			} else if (strcmp(property, "Context") == 0) {
				// Search conversation history for a topic string.
				// The topic is passed in the message's "data" field.
				const char* topic = "";
				msg->FindString("data", &topic);
				MainWindow* mw = _FindMainWindow();
				std::string ctx = mw ? mw->SearchContext(topic ? topic : "")
				                     : std::string();
				reply.AddString("result", ctx.c_str());
				reply.AddInt32("error", B_OK);
			} else {
				BApplication::MessageReceived(msg);
				break;
			}
			msg->SendReply(&reply);
			break;
		}

		case ipc::kMsgPeerAsk:
		case ipc::kMsgPeerDelegate:
		{
			// A peer sent us a prompt to execute. Forward it to MainWindow
			// as-is — MainWindow will inject it into the conversation.
			MainWindow* mw = _FindMainWindow();
			if (mw) {
				BMessenger(mw).SendMessage(msg);
			} else {
				// No window — send an error reply if possible.
				BMessage reply(B_ERROR);
				reply.AddString("error", "no main window");
				msg->SendReply(&reply);
			}
			break;
		}

		default:
			BApplication::MessageReceived(msg);
	}
}

bool App::_PromptProjectApproval(const McpServerSpec& spec) {
	BString body;
	body.SetToFormat(
		"Project MCP server “%s” requires approval.\n\n"
		"Type:    %s\n",
		spec.name.c_str(), spec.type.c_str());
	if (!spec.url.empty()) {
		BString line;
		line.SetToFormat("URL:     %s\n", spec.url.c_str());
		body += line;
	}
	if (!spec.command.empty()) {
		BString line;
		line.SetToFormat("Command: %s", spec.command.c_str());
		for (const auto& a : spec.args) { line += " "; line += a.c_str(); }
		line += "\n";
		body += line;
	}
	BString src;
	src.SetToFormat("Source:  %s", spec.source_path.c_str());
	body += src;

	BAlert* alert = new BAlert("Pluto — MCP approval",
		body.String(),
		"Deny", "Once", "Always",
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	int32 rc = alert->Go();

	if (rc == 0) return false;          // Deny
	if (rc == 2) pluto::SetProjectApproved(spec.name);
	return true;                        // Once or Always
}

// --- BeOS Scripting Protocol ---
// Exposes: Workspace (string, r), Model (string, r),
//          SessionTitle (string, r), Busy (bool, r),
//          Context (string, r) — search messages for a topic
// Testable via `hey Pluto-gui get Workspace`

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static property_info sPlutoProperties[] = {
	{ "Workspace",    { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
	  "Current working directory", 0, { B_STRING_TYPE } },
	{ "Model",        { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
	  "Active model ID", 0, { B_STRING_TYPE } },
	{ "SessionTitle", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
	  "Current session title", 0, { B_STRING_TYPE } },
	{ "Busy",         { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
	  "Whether a turn is in progress", 0, { B_BOOL_TYPE } },
	{ "Context",      { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
	  "Search conversation for a topic (pass topic in specifier data)", 0,
	  { B_STRING_TYPE } },
	{ 0 }
};
#pragma GCC diagnostic pop

MainWindow* App::_FindMainWindow() {
	for (int32 i = 0; i < CountWindows(); ++i) {
		auto* mw = dynamic_cast<MainWindow*>(WindowAt(i));
		if (mw) return mw;
	}
	return nullptr;
}

status_t App::GetSupportedSuites(BMessage* data) {
	data->AddString("suites", "suite/x-vnd.pluto-app");
	BPropertyInfo info(sPlutoProperties);
	data->AddFlat("messages", &info);
	return BApplication::GetSupportedSuites(data);
}

BHandler* App::ResolveSpecifier(BMessage* msg, int32 index,
                                 BMessage* specifier, int32 what,
                                 const char* property) {
	BPropertyInfo info(sPlutoProperties);
	if (info.FindMatch(msg, index, specifier, what, property) >= 0)
		return this;
	return BApplication::ResolveSpecifier(msg, index, specifier, what, property);
}

void App::_LaunchMain() {
	// Workspace trust prompt — if first time in this directory, ask the
	// user before allowing tool access. Runs synchronously (BAlert::Go).
	std::string ws = pluto::WorkspaceRoot();
	if (!ws.empty()
	    && !pluto::IsWorkspaceTrusted(ws)
	    && !pluto::IsSessionTrusted()) {
		BString text;
		text.SetToFormat(
			"Pluto wants to work in:\n\n"
			"  %s\n\n"
			"Tools will read and write files in this directory.\n"
			"Allow access?", ws.c_str());
		BAlert* alert = new BAlert("Pluto — workspace",
			text.String(),
			"Deny", "This session", "Always trust",
			B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		int32 rc = alert->Go();
		if (rc == 2) {
			pluto::TrustWorkspace(ws);
		} else if (rc == 1) {
			pluto::TrustSession();
		}
		// rc == 0 (Deny): tools will error on file access.
	}

	// Resolve credentials: OAuth first, then API key.
	fCreds = ResolveCredentials(*fConfig);
	bool needs_setup = fCreds.empty() && fApiKey.empty();

	if (fCreds.empty() && !fApiKey.empty()) {
		// API key passed from command line or legacy path.
		fCreds = { Credentials::API_KEY, fApiKey };
	}

	// No credentials: probe for Ollama or ask the user.
	bool open_prefs_after = false;
	if (needs_setup) {
		// Probe Ollama on localhost — zero-config if it's running.
		bool ollama_found = false;
		{
			CURL* probe = curl_easy_init();
			if (probe) {
				curl_easy_setopt(probe, CURLOPT_URL,
					"http://localhost:11434/api/tags");
				curl_easy_setopt(probe, CURLOPT_NOBODY, 1L);
				curl_easy_setopt(probe, CURLOPT_TIMEOUT, 1L);
				curl_easy_setopt(probe, CURLOPT_CONNECTTIMEOUT, 1L);
				CURLcode rc = curl_easy_perform(probe);
				long code = 0;
				curl_easy_getinfo(probe, CURLINFO_RESPONSE_CODE, &code);
				curl_easy_cleanup(probe);
				ollama_found = (rc == CURLE_OK && code == 200);
			}
		}

		OAuthTokens tok = LoadOAuthTokens();
		if (!tok.empty()) {
			// OAuth expired.
			BString body("Anthropic (Claude) OAuth session expired.\n\n"
				"Your Claude Max/Pro login token could not be refreshed.");
			if (ollama_found)
				body += "\n\nOllama detected on this machine — "
				        "you can use it immediately.";
			BAlert* alert = new BAlert("Pluto", body.String(),
				ollama_found ? "Use Ollama" : "Preferences",
				ollama_found ? "Preferences" : nullptr, nullptr,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			int32 choice = alert->Go();
			if (ollama_found && choice == 0) {
				fConfig->provider = "openai-compat";
				fConfig->base_url = "http://localhost:11434";
				fConfig->model    = "llama3.1";
				fConfig->api_key_env.clear();
				fConfig->context_window = 128000;
				pluto::SaveConfig(*fConfig);
				fCreds = { Credentials::API_KEY, "" };
				needs_setup = false;
			} else {
				open_prefs_after = true;
			}
		} else if (ollama_found) {
			// First run + Ollama detected — use it directly.
			BAlert* alert = new BAlert("Pluto",
				"Welcome to Pluto!\n\n"
				"Ollama detected on this machine. "
				"Use it now (free, private, no registration)?",
				"Use Ollama", "Preferences", nullptr,
				B_WIDTH_AS_USUAL, B_IDEA_ALERT);
			if (alert->Go() == 0) {
				fConfig->provider = "openai-compat";
				fConfig->base_url = "http://localhost:11434";
				fConfig->model    = "llama3.1";
				fConfig->api_key_env.clear();
				fConfig->context_window = 128000;
				pluto::SaveConfig(*fConfig);
				fCreds = { Credentials::API_KEY, "" };
				needs_setup = false;
			} else {
				open_prefs_after = true;
			}
		} else {
			// No Ollama, no credentials — welcome wizard.
			// Load .conf providers to find OpenRouter config.
			auto confs = pluto::LoadProviderConfs();
			int or_idx = -1;
			for (size_t i = 0; i < confs.size(); ++i) {
				if (confs[i].base_url.find("openrouter.ai") != std::string::npos)
					{ or_idx = (int)i; break; }
			}

			BAlert* alert = new BAlert("Pluto",
				"Welcome to Pluto!\n\n"
				"Quick start (no credit card needed):\n\n"
				"  OpenRouter — one free signup, many AI models\n"
				"    (Gemma, Llama, Qwen and more)\n\n"
				"  Ollama — install from HaikuDepot, fully offline\n\n"
				"Or choose from 15+ providers in Preferences.",
				"OpenRouter", "Ollama", "Preferences",
				B_WIDTH_AS_USUAL, B_IDEA_ALERT);
			int32 choice = alert->Go();
			if (choice == 0) {
				// OpenRouter — use .conf if available.
				fConfig->provider = "openai-compat";
				if (or_idx >= 0) {
					fConfig->base_url = confs[or_idx].base_url;
					fConfig->api_key_env = confs[or_idx].api_key_env;
					if (!confs[or_idx].models.empty()) {
						fConfig->model = confs[or_idx].models[0].id;
						fConfig->max_tokens = confs[or_idx].models[0].max_tokens;
						fConfig->context_window = confs[or_idx].models[0].context_window;
					}
				} else {
					fConfig->base_url = "https://openrouter.ai/api";
					fConfig->model = "google/gemma-4-31b-it:free";
					fConfig->api_key_env = "OPENROUTER_API_KEY";
					fConfig->context_window = 131072;
				}
				pluto::SaveConfig(*fConfig);
				fCreds = { Credentials::API_KEY, "" };
				needs_setup = false;
				// Open preferences so the user can enter their API key.
				open_prefs_after = true;
			} else if (choice == 1) {
				// Ollama — zero config.
				fConfig->provider = "openai-compat";
				fConfig->base_url = "http://localhost:11434";
				fConfig->model = "llama3.1";
				fConfig->api_key_env.clear();
				fConfig->context_window = 128000;
				pluto::SaveConfig(*fConfig);
				fCreds = { Credentials::API_KEY, "" };
				needs_setup = false;
			} else {
				// Preferences — let the user pick manually.
				open_prefs_after = true;
			}
		}
	}

	// Respect the saved provider choice. OAuth tokens being present does
	// not override an explicit openai-compat provider in the config.
	if (fCreds.type == Credentials::OAUTH
	    && fConfig->provider == "openai-compat"
	    && !fConfig->base_url.empty()) {
		// User explicitly chose openai-compat — use API key for that,
		// keep OAuth tokens on disk for when they switch back to Anthropic.
		std::string key = ResolveApiKey(*fConfig);
		fCreds = { Credentials::API_KEY, key };
	}

	// Create the provider from the saved config.
	if (fConfig->provider == "openai-compat"
	    && !fConfig->base_url.empty()) {
		fProvider = std::make_unique<OpenAICompatProvider>(
			fCreds.empty() ? "" : fCreds.token, fConfig->base_url);
	} else if (!fCreds.empty()) {
		AuthType auth = (fCreds.type == Credentials::OAUTH)
			? AuthType::OAUTH : AuthType::API_KEY;
		fProvider = std::make_unique<AnthropicProvider>(fCreds.token, auth);
	} else {
		// No credentials yet — create a placeholder provider.
		// It will be replaced when the user configures one via Settings.
		fProvider = std::make_unique<AnthropicProvider>("", AuthType::API_KEY);
	}
fMcp = std::make_unique<McpManager>(*fTools,
		[this](const McpServerSpec& s) { return _PromptProjectApproval(s); });

	// Auto-discover local MCP servers not yet configured and connect them
	// for this session. No manual `pluto mcp add` needed.
	for (const auto& d : pluto::DiscoverMcpServers()) {
		McpServerSpec spec;
		spec.name = d.name;
		spec.type = d.type;
		spec.url = d.url;
		spec.scope = McpServerSpec::LOCAL;
		std::string err;
		auto transport = pluto::MakeTransport(spec, err);
		if (!transport || !transport->start(err)) continue;
		auto client = std::make_shared<pluto::McpClient>(
			spec.name, std::move(transport));
		if (!client->connect()) continue;
		std::fprintf(stderr, "[discover/%s] %zu tools\n",
			spec.name.c_str(), client->tools().size());
		fMcp->add_client(client, *fTools);
	}

	// Inject MCP tool descriptions into the system prompt so the model
	// knows what remote tools are available and when to use them.
	{
		std::string mcp_block = fMcp->system_prompt_block();
		if (!mcp_block.empty()) {
			if (!fConfig->system_prompt.empty()) fConfig->system_prompt += "\n\n";
			fConfig->system_prompt += mcp_block;
		}
	}

	auto* win = new MainWindow(fProvider.get(), fConfig, fTools, fMcp->clients());
	win->Show();

	// Open Preferences if the user chose manual setup.
	if (open_prefs_after) {
		// kMsgPrefs = 'plPr' (defined in MainWindow.cpp)
		BMessenger(win).SendMessage(static_cast<uint32>('plPr'));
	}

	// Deliver any B_REFS_RECEIVED that arrived before the window existed
	// (e.g. Tracker add-on launched the app with file refs).
	if (fHasPendingRefs) {
		BMessenger(win).SendMessage(&fPendingRefs);
		fHasPendingRefs = false;
	}
}

}  // namespace gui
}  // namespace pluto
