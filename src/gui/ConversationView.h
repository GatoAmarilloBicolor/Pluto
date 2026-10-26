#pragma once
// ConversationView — per-conversation BView for the multi-tab refactor.
//
// Owns:  fDocView (chat text), fMessages, fToolBlocks, fStreamingMd,
//        fWorker (agent thread), token counters, session metadata.
// Does NOT own: input field, side panel, menus, provider, config, tools.
//
// R1 = header + skeleton. Methods are stubs. MainWindow does not use this
// class yet — integration happens in R2/R2b.

#include <Bitmap.h>
#include <SupportDefs.h>
#include <View.h>

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"
#include "ToolBlockManager.h"
#include "StreamingMarkdown.h"
#include "textview/TextDocumentView.h"
#include "textview/MarkdownParser.h"

class BMessageRunner;
class BScrollView;
namespace pluto {

class McpClient;

namespace gui {

// Callback interface: ConversationView → MainWindow.
// MainWindow implements this to react to events from the active tab.
struct ConversationDelegate {
	virtual ~ConversationDelegate() = default;

	// Status bar + gauge should be refreshed.
	virtual void OnStatusChanged() = 0;

	// Session title changed — update window title bar.
	virtual void OnTitleChanged(const std::string& title) = 0;

	// Turn started — disable input, show "thinking…".
	virtual void OnTurnStarted() = 0;

	// Title bar activity indicator ("writing…", "running tool_name").
	virtual void OnTitleActivity(const std::string& activity) = 0;

	// Tool produced output — update side panel.
	virtual void OnToolOutput(const std::string& tool_name,
	                          const std::string& body,
	                          const std::string& input_json,
	                          bool is_error) = 0;

	// Image received (base64) — show in side panel preview.
	virtual void OnImage(const std::string& base64_data,
	                     const std::string& media_type) = 0;

	// Turn completed — re-enable Send, trigger auto-save, notification.
	// |preview| is the last assistant text for notification.
	virtual void OnTurnDone(const std::string& preview) = 0;

	// Tool approval request (cross-thread, called from worker).
	// Returns 0 = deny, 1 = allow, 2 = always.
	virtual int RequestToolApproval(const std::string& tool_name,
	                                const std::string& input_preview) = 0;

	// Error from provider or tool — show alert/notification.
	virtual void OnError(const std::string& error) = 0;

	// RAG injection occurred — show topics in side panel.
	virtual void OnRagInject(const std::vector<std::string>& topics) = 0;

	// Retry / status message from provider.
	virtual void OnProviderStatus(const std::string& status) = 0;
};

class ConversationView : public BView {
public:
	// Each tab owns its own Config copy and Provider instance.
	// |cfg| is copied; |provider| is adopted (ownership transferred).
	ConversationView(std::unique_ptr<Provider> provider, const Config& cfg,
	                 ToolRegistry* tools, ConversationDelegate* delegate);
	~ConversationView() override;

	void AttachedToWindow() override;
	void FrameResized(float width, float height) override;
	void MessageReceived(BMessage* msg) override;

	// --- Public API (called by MainWindow) ---

	// Inject user content blocks, start an agent turn.
	void SendInput(std::vector<ContentBlock> blocks);

	// Cancel the in-flight turn (if busy).
	void Cancel();

	// Clear conversation history.
	void Reset();

	// Load a set of messages (e.g. for session branching/fork).
	// Replaces current messages and re-renders.
	void LoadMessages(const std::vector<Message>& msgs);

	// Load a saved session from a JSONL file.
	void LoadSession(const std::string& path);

	// Save the current session (or update existing).
	void SaveSession();

	// Getters for IPC and status bar.
	std::string GetSessionTitle() const { return fSessionTitle; }
	bool        IsBusy() const          { return fBusy; }
	long        GetTokensIn() const     { return fTokensIn; }
	long        GetTokensOut() const    { return fTokensOut; }
	long        GetLastInput() const    { return fLastInput; }
	long        GetLastOutput() const   { return fLastOutput; }
	long        GetLastCacheRead() const   { return fLastCacheRead; }
	long        GetLastCacheCreate() const { return fLastCacheCreate; }
	int         GetTurnCount() const    { return fTurnCount; }
	size_t      GetMessageCount() const { return fMessages.size(); }
	bigtime_t   GetTurnStartedAt() const { return fTurnStartedAt; }
	double      EstimateCost() const;

	// Request conversation compaction (manual /compact command).
	void RequestCompaction();

	// Per-tab config access (model, max_tokens, context_window, etc.).
	const Config& GetConfig() const { return fTabConfig; }
	Config&       MutableConfig()   { return fTabConfig; }
	std::string   GetModel() const  { return fTabConfig.model; }

	// Replace this tab's provider (e.g. after model switch).
	void SetProvider(std::unique_ptr<Provider> provider);
	Provider* GetProvider() const { return fProvider; }

	// Session metadata management.
	void SetSessionTitle(const std::string& title);
	std::string GetSessionPath() const { return fSessionPath; }
	void SetSessionPath(const std::string& path) { fSessionPath = path; }

	// Insert a base64-encoded image inline in the chat.
	void InsertInlineImage(const std::string& base64Data)
		{ _InsertInlineImage(base64Data); }

	// Access the output views.
	// Legacy OutputView() returns nullptr — use DocView() instead.
	TextDocumentView* DocView() const { return fDocView; }
	TextDocumentRef GetTextDocument() const { return fTextDoc; }
	BScrollView* OutputScroll() const { return fOutputScroll; }

	// Streaming cursor tick (called from MainWindow's BMessageRunner).
	void OnCursorTick();

	// Font zoom (applied from MainWindow).
	void ApplyFontDelta(int delta);

	// Update MarkdownParser colors for forced dark/light schemes.
	void UpdateParserColors(bool useSystem,
	                        rgb_color codeFg, rgb_color codeBg,
	                        rgb_color linkColor);

	// Per-tab image preview bitmap (for save/restore on tab switch).
	BBitmap* PreviewBitmap() const { return fPreviewBitmap; }
	void     SetPreviewBitmap(BBitmap* bmp) { delete fPreviewBitmap; fPreviewBitmap = bmp; }

	// Access messages for export.
	const std::vector<Message>& Messages() const { return fMessages; }

	// Build markdown export of the conversation.
	std::string BuildMarkdown() const;

	// Find text in the conversation output (called by MainWindow on behalf
	// of the find bar, since ConversationView does not own fFindBar).
	void FindInConversation(const std::string& query, bool forward);

	// --- Worker event handlers (called by MainWindow message handlers) ---
	// These perform all rendering and state updates internally.

	// Handle streaming text delta. Returns true if this was the first
	// chunk (a new assistant block was started).
	bool HandleTextDelta(const std::string& text);

	// Handle tool_use start (tool header block).
	void HandleToolStart(const std::string& name, const std::string& preview);

	// Handle tool_result. Returns the current tool name (for side panel).
	std::string HandleToolResult(const std::string& output, bool is_error);

	// Handle error from the provider.
	void HandleError(const std::string& error);

	// Handle token usage update.
	void HandleUsage(long in, long out, long cache_read, long cache_create);

	// Handle turn completion. Returns last assistant text (for notification).
	std::string HandleTurnDone();

	// Handle provider status/retry message. is_warning = no-tools warning.
	void HandleStatusMessage(const std::string& status, bool is_warning);

	// Handle RAG injection hint line in chat.
	void HandleRagHint(const std::string& hint);

	// Prepare for a new turn: push user message, reset turn state.
	void PrepareTurn(std::vector<ContentBlock> user_blocks);

	// Mutable access to messages (for worker thread).
	std::vector<Message>& MutableMessages() { return fMessages; }

	// Current tool name (for side panel routing after HandleToolResult).
	std::string GetCurrentTool() const { return fCurrentTool; }

	// Append styled text to output (for banner, deskbar messages, etc.).
	void AppendText(const std::string& text, rgb_color color,
	                uint16 face = 0, bool mono = false);

private:
	// --- Worker thread ---
	void _StartTurn();
	static int32 _WorkerEntry(void* data);

	// --- Rendering helpers ---
	void _Append(const std::string& text, rgb_color color,
	             uint16 face = 0, bool mono = false);
	void _EmitUserHeader();
	void _EmitAssistantHeader();
	void _FinalizeAssistantBlock();
	void _RenderAssistantInline(const std::string& line);
	void _FlushMdCodeBlockImpl(const std::string& code, const std::string& lang);
	void _RenderAssistantLine(const std::string& line);
	void _FeedStreamingMarkdown(const std::string& chunk);
	void _FlushStreamingMarkdown();
	void _ResetStreamingMarkdown();
	void _RenderAssistantMarkdown(const std::string& text);
	void _ShowCursor();
	void _HideCursor();
	void _RenderMessages();
	void _InsertInlineImage(const std::string& base64_data);
	void _ClearInlineImages();
	void _RepositionInlineImages();

	// --- Per-conversation state ---

	// UI
	TextDocumentView* fDocView      = nullptr;   // markdown-native view
	TextDocumentRef   fTextDoc;                  // document model for fDocView
	MarkdownParser    fMdParser;                 // markdown → document converter
	Paragraph         fCurrentPara;              // accumulates spans until \n
	BScrollView*      fOutputScroll = nullptr;
	BMessageRunner*   fCursorRunner = nullptr;

	// Conversation
	std::vector<Message>   fMessages;
	ToolBlockManager       fToolBlocks;
	int                    fPendingTool   = -1;

	// Streaming markdown
	StreamingMarkdown      fStreamingMd;
	int32                  fMdPreviewLen  = 0;

	// Worker thread
	thread_id              fWorker        = -1;
	std::atomic<bool>      fCancelFlag{false};  // per-tab cancel token
	bool                   fBusy          = false;
	bool                   fTextOngoing   = false;

	// Font zoom (delta from system font size)
	int                    fFontDelta     = 0;

	// Rendering state
	bool                   fCursorShown   = false;
	bool                   fThinking      = false;  // true before first text arrives
	int                    fSpinnerFrame  = 0;
	int32                  fShimmerStart  = -1;
	int32                  fAsstHeaderOff = -1;
	int32                  fAsstBodyOff   = -1;
	std::string            fCurrentTool;
	std::string            fLastAssistantText;
	std::string            fAsstMarkdownBuf;    // accumulated markdown for current assistant block
	int32                  fAsstDocParaStart = 0; // paragraph index where assistant content starts
	int32                  fAsstHeaderPara = -1;  // paragraph index of assistant header
	int                    fPendingAsstBlock = -1;  // index in fToolBlocks
	int32                  fFindOffset    = 0;

	// Token accounting
	long                   fTokensIn      = 0;
	long                   fTokensOut     = 0;
	long                   fLastInput     = 0;
	long                   fLastOutput    = 0;
	long                   fLastCacheRead = 0;
	long                   fLastCacheCreate = 0;
	int                    fTurnCount     = 0;

	// Session metadata
	std::string            fSessionPath;
	std::string            fSessionTitle;
	bigtime_t              fTurnStartedAt = 0;

	// Tools that the user approved with "Always" — skip future prompts.
	std::set<std::string>  fAlwaysApproved;

	// Per-tab image preview bitmap (owned, saved/restored on tab switch).
	BBitmap*                        fPreviewBitmap = nullptr;

	// Inline image views embedded in the chat output.
	std::vector<BView*>             fInlineImages;
	std::vector<int32>              fImageOffsets;

	// Per-tab owned config and provider.
	Config                          fTabConfig;
	std::unique_ptr<Provider>       fOwnedProvider;
	Provider*                       fProvider;  // == fOwnedProvider.get()
	Config*                         fConfig;    // == &fTabConfig

	// Shared references (not owned)
	ToolRegistry*          fTools;
	ConversationDelegate*  fDelegate;
};

}  // namespace gui
}  // namespace pluto
