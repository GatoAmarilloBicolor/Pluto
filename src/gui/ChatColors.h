#pragma once
// ChatColors.h — Arrow glyphs, cursor, shared message constants, and
// helper BTextView subclasses for both MainWindow and ConversationView.

#include <InterfaceDefs.h>
#include <Message.h>
#include <SupportDefs.h>
#include <TextView.h>
#include <Window.h>

namespace pluto {
namespace gui {

// Arrow glyphs for collapsible blocks (3 bytes each in UTF-8).
extern const char* kArrowOpen;   // ▾
extern const char* kArrowClose;  // ▸
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

}  // namespace gui
}  // namespace pluto
