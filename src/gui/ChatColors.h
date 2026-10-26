// ChatColors.h — Native Haiku color palette, shared message constants,
// and helper BTextView subclasses for both MainWindow and ConversationView.

#pragma once

#include <InterfaceDefs.h>
#include <Message.h>
#include <SupportDefs.h>
#include <TextView.h>
#include <Window.h>

#include <string>

namespace pluto {
namespace gui {

// Arrow glyphs for collapsible blocks (3 bytes each in UTF-8).
extern const char* kArrowOpen;   // ▼
extern const char* kArrowClose;  // ▶
extern const char* kCursorChar;  // █
extern const int32 kUtf3;        // 3

// Shared message constants (inline constexpr for switch-case labels).
inline constexpr uint32 kMsgToolToggle     = 'plTt';
inline constexpr uint32 kMsgWorkerText     = 'plTx';
inline constexpr uint32 kMsgWorkerTS       = 'plTs';
inline constexpr uint32 kMsgWorkerTR       = 'plTr';
inline constexpr uint32 kMsgWorkerErr      = 'plEr';
inline constexpr uint32 kMsgWorkerDone     = 'plDn';
inline constexpr uint32 kMsgWorkerUsage    = 'plUs';
inline constexpr uint32 kMsgWorkerImage    = 'plWi';
inline constexpr uint32 kMsgWorkerStatus   = 'plWs';
inline constexpr uint32 kMsgWorkerRag      = 'plWr';
inline constexpr uint32 kMsgApproval       = 'plAp';
inline constexpr uint32 kMsgCursorTick     = 'plCt';
inline constexpr uint32 kMsgLinkClick      = 'link';  // from MarkdownParser
inline constexpr uint32 kMsgShowSettings   = 'plSt';
inline constexpr uint32 kMsgModelPicker    = 'plMp';
inline constexpr uint32 kMsgNewTab         = 'plNt';
inline constexpr uint32 kMsgCloseTab       = 'plCl';
inline constexpr uint32 kMsgFullScreen     = 'plFs';
inline constexpr uint32 kMsgZoomIn         = 'plZi';
inline constexpr uint32 kMsgZoomOut        = 'plZo';
inline constexpr uint32 kMsgZoomReset      = 'plZr';
inline constexpr uint32 kMsgReset          = 'plRs';
inline constexpr uint32 kMsgAbout          = 'plAb';
inline constexpr uint32 kMsgExportMd       = 'plEm';
inline constexpr uint32 kMsgExportJsonl    = 'plEj';

// ---------------------------------------------------------------------------
// Native Haiku color palette using ui_color() for true dark/light support.
// These are computed once at startup from the system colors.
// ---------------------------------------------------------------------------

struct NativePalette {
	// Backgrounds
	rgb_color window_bg;       // B_DOCUMENT_BACKGROUND_COLOR
	rgb_color panel_bg;        // B_PANEL_BACKGROUND_COLOR
	rgb_color control_bg;      // B_CONTROL_BACKGROUND_COLOR
	rgb_color list_bg;         // B_LIST_BACKGROUND_COLOR
	rgb_color list_sel_bg;     // B_LIST_SELECTED_BACKGROUND_COLOR
	rgb_color header_bg;       // custom: slightly lighter than window

	// Text
	rgb_color text_fg;         // B_DOCUMENT_TEXT_COLOR
	rgb_color text_dim;        // dimmed text
	rgb_color text_inv;        // text on selected bg
	rgb_color link_color;      // custom: system link blue

	// Borders
	rgb_color border;          // B_PANEL_TEXT_COLOR (subtle)
	rgb_color focus_border;    // B_KEYBOARD_NAVIGATION_COLOR

	// Status / accent
	rgb_color accent;          // B_LINK_TEXT_COLOR or custom
	rgb_color success;         // green
	rgb_color warning;         // amber
	rgb_color error;           // red
	rgb_color info;            // blue
};

// Global palette instance — initialized in MainWindow::_InitPalette().
extern NativePalette gPalette;

// Initialize palette from Haiku system colors (ui_color()).
void InitNativePalette();

// Convenience: lighten/darken a color.
rgb_color Lighten(rgb_color c, int by);
rgb_color Darken(rgb_color c, int by);

// Read-only conversation view with clickable tool-block headers.
// Also forwards file drops to the window.
class ChatOutput : public BTextView {
public:
	ChatOutput() : BTextView("output") {
		MakeEditable(false);
		SetStylable(true);
		SetWordWrap(true);
		SetInsets(10, 10, 10, 10);
	}

	void MouseDown(BPoint p) override {
		int32 offset = OffsetAt(p);
		BMessage m(kMsgToolToggle);
		m.AddInt32("offset", offset);
		m.AddFloat("x", p.x);
		if (Window()) Window()->PostMessage(&m);
		BTextView::MouseDown(p);
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == B_SIMPLE_DATA || msg->what == B_REFS_RECEIVED) {
			if (Window()) {
				BMessage forward(*msg);
				forward.what = B_REFS_RECEIVED;
				Window()->PostMessage(&forward);
			}
			return;
		}
		BTextView::MessageReceived(msg);
	}
};

// Thin status label that updates color based on state.
class StatusLabel : public BStringView {
public:
	StatusLabel(const char* name, const char* text)
		: BStringView(name, text) {}

	void SetState(const char* state) {
		if (strcmp(state, "idle") == 0) {
			SetHighColor(gPalette.text_fg);
		} else if (strcmp(state, "busy") == 0) {
			SetHighColor(gPalette.accent);
		} else if (strcmp(state, "error") == 0) {
			SetHighColor(gPalette.error);
		} else if (strcmp(state, "success") == 0) {
			SetHighColor(gPalette.success);
		}
		Invalidate();
	}
};

}  // namespace gui
}  // namespace pluto
