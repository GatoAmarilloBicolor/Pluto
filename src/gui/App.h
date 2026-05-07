#pragma once

#include <Application.h>
#include <SupportDefs.h>

#include <memory>
#include <string>
#include <vector>

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"

namespace pluto {

class McpClient;
class McpManager;
struct McpServerSpec;

namespace gui {

class MainWindow;

class App : public BApplication {
public:
	App(Config* cfg, ToolRegistry* tools, std::string initial_key);
	~App() override;

	void ReadyToRun() override;
	void RefsReceived(BMessage* msg) override;
	void MessageReceived(BMessage* msg) override;

	// BeOS scripting protocol — expose app state to other instances.
	status_t GetSupportedSuites(BMessage* data) override;
	BHandler* ResolveSpecifier(BMessage* msg, int32 index,
	                           BMessage* specifier, int32 what,
	                           const char* property) override;

private:
	void _LaunchMain();
	bool _PromptProjectApproval(const McpServerSpec& spec);
	MainWindow* _FindMainWindow();

	Config*       fConfig;
	ToolRegistry* fTools;
	std::string   fApiKey;       // API key (may be empty if using OAuth)
	Credentials   fCreds;        // resolved credentials (OAuth or API key)
	std::unique_ptr<Provider> fProvider;
	std::unique_ptr<McpManager> fMcp;
	BMessage      fPendingRefs;          // stashed B_REFS_RECEIVED
	bool          fHasPendingRefs = false;
};

}  // namespace gui
}  // namespace pluto
