// Pluto-gui — Haiku-native GUI front-end sketch.

#include <image.h>
#include <OS.h>
#include <curl/curl.h>

#include <cstring>
#include <string>
#include <unistd.h>

#include "App.h"

#include "../config.h"
#include "../oauth.h"
#include "../rag.h"
#include "../haiku/BfsTools.h"
#include "../storage/SessionStore.h"
#include "../workspace.h"
#include "../haiku/DesktopTools.h"
#include "../tools/BeapiLookup.h"
#include "../tools/Builtin.h"
#include "../tools/Tool.h"
#include "../tools/WebFetch.h"
#include "../tools/WebSearch.h"

int main(int argc, char** argv) {
	// Check for --workspace= argument (set by Ask Pluto Tracker add-on).
	std::string initial_workspace;
	for (int i = 1; i < argc; ++i) {
		std::string arg(argv[i]);
		if (arg.rfind("--workspace=", 0) == 0) {
			initial_workspace = arg.substr(12);
		}
	}

	if (!initial_workspace.empty()) {
		// Ask Pluto told us which directory to work in.
		chdir(initial_workspace.c_str());
	} else {
		// When launched from Tracker (double-click), the cwd is typically
		// /boot/home — not the directory containing the binary. Change to
		// the binary's directory so that WorkspaceInit() picks up the
		// correct project root and .mcp.json is found.
		image_info info;
		int32 cookie = 0;
		while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
			if (info.type == B_APP_IMAGE) {
				char* slash = std::strrchr(info.name, '/');
				if (slash) {
					*slash = '\0';
					chdir(info.name);
				}
				break;
			}
		}
	}

	pluto::WorkspaceInit();

	// Use project-local session storage when inside a workspace.
	{
		std::string ws = pluto::WorkspaceRoot();
		if (!ws.empty()) {
			pluto::SessionStore::set_dir_override(
				ws + "/.pluto/sessions");
		}
	}

	pluto::Config cfg = pluto::LoadConfig();
	pluto::AutoDetectProvider(cfg);
	pluto::ApplyProjectMemory(cfg);
	pluto::ApplySkills(cfg);
	pluto::LoadModes(cfg);

	// Load BeAPI RAG index.
	std::string rag_path = pluto::RagDefaultPath();
	if (!rag_path.empty()) {
		int n = pluto::RagInit(rag_path);
		if (n > 0) {
			pluto::RagLoadCustom(pluto::RagCustomPath());
			pluto::ApplyRagHint(cfg);
		}
	}

	// Try OAuth first, then API key. If empty, Preferences opens on launch.
	std::string key;
	pluto::Credentials creds = pluto::ResolveCredentials(cfg);
	if (!creds.empty())
		key = creds.token;  // App will re-resolve to get the type

	curl_global_init(CURL_GLOBAL_DEFAULT);

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
	tools.add(pluto::MakeLiveQueryTool());
	tools.add(pluto::MakeClipboardTool());
	tools.add(pluto::MakeNodeMonitorTool());
	tools.add(pluto::MakeDesktopScriptTool());
	tools.add(pluto::MakeLaunchAppTool());
	if (pluto::RagEntryCount() > 0)
		tools.add(pluto::MakeBeapiLookupTool());
	tools.add(pluto::MakeWebFetchTool());
	tools.add(pluto::MakeWebSearchTool());
	tools.add(pluto::MakeTodoWriteTool());
	tools.add(pluto::MakeApplyPatchTool());
	tools.add(pluto::MakeLspTool());

	pluto::gui::App app(&cfg, &tools, key);
	app.Run();

	curl_global_cleanup();
	return 0;
}
