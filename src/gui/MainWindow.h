#pragma once

#include <InterfaceDefs.h>
#include <Window.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../base64.h"
#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"
#include "ConversationView.h"
#include "MiniMapView.h"

class BButton;
class BGroupView;
class BMenu;
class BMenuBar;
class BScrollView;
class BSplitView;
class BStringView;
class BTabView;
class BTextControl;
class BTextView;

namespace pluto {

class McpClient;

namespace gui {

class ImagePreviewView;
class TokenGauge;

class MainWindow : public BWindow, public ConversationDelegate {
public:
	MainWindow(Provider* provider, Config* cfg, ToolRegistry* tools,
	           const std::vector<std::shared_ptr<McpClient>>& mcp_clients);
	~MainWindow() override;

	void MessageReceived(BMessage* msg) override;
	bool QuitRequested() override;

	// Public getters for IPC scripting protocol (delegate to ConversationView).
	std::string GetSessionTitle() const;
	std::string GetActiveModel() const;
	bool        IsBusy() const;
	// Search all tabs for messages mentioning `topic`. Returns relevant
	// text extracts (max ~2000 chars) for context sharing between peers.
	std::string SearchContext(const std::string& topic) const;

private:
	struct LinkRegion {
		int32       start = 0;
		int32       end   = 0;
		std::string url;
	};

	struct PendingImage {
		std::string path;
		std::string filename;
		std::string media_type;
		std::string base64_data;
		std::string kind;   // "image", "audio", "pdf" — for chip icon
	};

	void _BuildMenuBar();
	void _RebuildMcpMenu();
	void _RebuildSessionsMenu();
	void _ShowMcpServerInfo(int32 index);

	struct CustomCommand {
		std::string name;
		std::string description;
		std::string body;
	};

	bool _HandleSlashCommand(const std::string& text);
	void _SendCurrentInput();
	void _ResetConversation();
	void _ShowAbout();

	// Tab management.
	static std::string _BuildTabLabel(ConversationView* conv);
	ConversationView* _CreateTab();
	void _CloseTab();
	void _CloseOtherTabs(int32 keep);
	void _RenameTab(int32 idx);
	void _OnTabChanged();
	void _UpdateMiniMap();
	void _SaveWorkspace();
	bool _RestoreWorkspace();

	void _SaveSession();
	void _LoadSessionFromPath(const std::string& path);

	void _SaveImageToFile(const entry_ref* dir, const char* name);
	void _OpenModelPicker();
	void _OpenSettings();
	void _ApplyFontZoom();
	void _SetSendStopMode(bool stopping);
	void _AttachFileToInput(const std::string& path);
	void _SaveBitmapAndAttach(BBitmap* bmp);
	void _HandleBitmapClipDrop(const void* data, ssize_t size);
	void _AttachImageToInput(const std::string& path,
	                         const std::string& media_type);
	void _AttachMediaToInput(const std::string& path,
	                         const std::string& format,
	                         const std::string& kind);
	void _ClearPendingImages();
	void _UpdateImageChips();
	void _LoadImagePreviewFromBase64(const std::string& base64_data);
	void _InitPalette();       // recompute colors from ui_color()
	void _ApplyViewColors();   // apply bg/fg to chat views

	void _RenderEditDiff(const std::string& input_json);
	void _RenderMarkdown(const std::string& text);
	void _ExportMarkdown();
	void _ExportJsonl();

	void _FindInConversation(bool forward = true);
	void _ShowFindBar();
	void _HideFindBar();
	void _UpdateTitle(const char* activity = nullptr);

	void _UpdateStatus();

	// Active tab helper — returns the ConversationView for the selected tab.
	ConversationView* _ActiveConv() const;

	// Thin wrappers — delegate to active ConversationView for output.
	void _Append(const std::string& text, rgb_color color,
	             uint16 face = 0, bool mono = true);
	BView* _Output() const;

	// Side panel
	void _ResetSidePanel();
	void _UpdateSidePanel(const std::string& title,
	                      const std::string& body,
	                      bool is_error);

	// ConversationDelegate implementation.
	void OnStatusChanged() override;
	void OnTitleChanged(const std::string& title) override;
	void OnTurnStarted() override;
	void OnTitleActivity(const std::string& activity) override;
	void OnToolOutput(const std::string& tool_name,
	                  const std::string& body,
	                  const std::string& input_json,
	                  bool is_error) override;
	void OnImage(const std::string& base64_data,
	             const std::string& media_type) override;
	void OnTurnDone(const std::string& preview) override;
	int  RequestToolApproval(const std::string& tool_name,
	                         const std::string& input_preview) override;
	void OnRagInject(const std::vector<std::string>& topics) override;
	void OnProviderStatus(const std::string& status) override;

	BTabView*     fTabView      = nullptr;
	gui::MiniMapView* fMiniMap = nullptr;
	BMenuBar*     fMenuBar      = nullptr;
	BMenu*        fMcpMenu      = nullptr;
	BMenu*        fSessionsMenu = nullptr;
	BSplitView*   fSplitView    = nullptr;
	BTextView*    fInput        = nullptr;
	BScrollView*  fInputScroll  = nullptr;
	BStringView*  fSideTitle    = nullptr;
	BTextView*    fSideView     = nullptr;
	BScrollView*  fSideScroll   = nullptr;
	ImagePreviewView*  fImagePreview = nullptr;
	// Mini audio player in side panel.
	BGroupView*   fPlayerBar     = nullptr;
	BButton*      fPlayerPlay    = nullptr;
	BButton*      fPlayerStop    = nullptr;
	BStringView*  fPlayerTime    = nullptr;
	std::string   fPlayerPath;                   // currently loaded file
	ConversationView*  fPrevConv     = nullptr;
	BButton*      fSend         = nullptr;
	BStringView*  fStatus       = nullptr;
	TokenGauge*   fGauge        = nullptr;
	BTextControl* fFindBar      = nullptr;
	int32         fFindOffset    = 0;

	Provider*     fProvider;
	std::unique_ptr<Provider> fOwnedProvider;  // when we recreate provider at runtime
	Config*       fConfig;
	ToolRegistry* fTools;
	std::vector<std::shared_ptr<McpClient>> fMcpClients;

	std::vector<LinkRegion>            fSideLinks;
	std::map<std::string, int32>       fSideAnchors;

	std::set<std::string> fAlwaysApproved; // tools to skip approval prompt
	std::vector<PendingImage> fPendingImages;
	BGroupView*          fImageChips    = nullptr;
	std::vector<CustomCommand> fCustomCommands;
	int                  fFontDelta     = 0;
	int                  fCtxWarningShown = 0; // highest ctx % threshold notified
	std::string          fInitPendingPath;  // pluto.md path awaiting LLM generation
	size_t               fInitMsgCount = 0; // message count before init turn
};

}  // namespace gui
}  // namespace pluto
