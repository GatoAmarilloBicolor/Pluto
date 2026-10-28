#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Autolock.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <Font.h>
#include <TextControl.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <File.h>
#include <FilePanel.h>
#include <Directory.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <TabView.h>
#include <TranslatorRoster.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Notification.h>
#include <PopUpMenu.h>
#include <Roster.h>

#include <curl/curl.h>
#include <Path.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <set>

#include "../agent/Loop.h"
#include "../cancel.h"
#include "../input.h"
#include "../ipc/PlutoMessenger.h"
#include "../telemetry.h"
#include "../json.h"
#include "ChatColors.h"
#include "../mcp/Client.h"
#include "../mcp/Config.h"
#include "../provider/Anthropic.h"
#include "../provider/OpenAICompat.h"
#include "../rag.h"
#include "../routine.h"
#include "../util.h"
#include "../workspace.h"
#include "../storage/SessionStore.h"
#include "../haiku/Notifications.h"
#include "DeskbarReplicant.h"
#include "AgentStoreDialog.h"
#include "RoutineDialog.h"
#include "ScheduleDialog.h"
#include "SessionBrowser.h"
#include "SettingsDialog.h"
#include "GraphWindow.h"
#include "TelemetryDialog.h"
#include "../oauth.h"
#include "../schedule.h"
#include "SyntaxHighlight.h"

#include <dirent.h>
#include <image.h>
#include <sys/stat.h>

#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>

#include "../wiki.h"

namespace pluto {
namespace gui {

namespace {

const uint32 kMsgSend         = 'plSd';
const uint32 kMsgReset        = 'plRs';
const uint32 kMsgAbout        = 'plAb';
const uint32 kMsgMcpInfo      = 'plMi';
const uint32 kMsgSaveSession  = 'plSv';
const uint32 kMsgResumeLast   = 'plRl';
const uint32 kMsgLoadSession  = 'plLd';
const uint32 kMsgStop         = 'plSt';
const uint32 kMsgZoomIn       = 'plZi';
const uint32 kMsgZoomOut      = 'plZo';
const uint32 kMsgZoomReset    = 'plZr';
const uint32 kMsgPrefs        = 'plPr';
const uint32 kMsgExportMd     = 'plEm';
const uint32 kMsgExportJsonl  = 'plEj';
const uint32 kMsgFind         = 'plFn';
const uint32 kMsgFindNext     = 'plF>';
const uint32 kMsgFindHide     = 'plFh';
const uint32 kMsgMcpDiscover  = 'plMd';
const uint32 kMsgUserGuide   = 'plUg';
const uint32 kMsgSideLink     = 'plSl';
const uint32 kMsgDeskbarInstall = 'plDi';
const uint32 kMsgDeskbarRemove  = 'plDr';
const uint32 kMsgTrackerAddonInstall = 'plTi';
const uint32 kMsgTrackerAddonRemove  = 'plTR';
const uint32 kMsgScheduleDialog      = 'plSc';
const uint32 kMsgRoutineDialog       = 'plRd';
const uint32 kMsgAgentDialog         = 'plAg';
const uint32 kMsgImageClear          = 'plIc';
const uint32 kMsgImageRemove         = 'plIr';
const uint32 kMsgSessionBrowser      = 'plSb';
const uint32 kMsgModelQuickSwitch    = 'plMq';
const uint32 kMsgRunRoutine          = 'plRr';
const uint32 kMsgOAuthLogin          = 'plOl';
const uint32 kMsgOAuthLogout         = 'plOo';
const uint32 kMsgOAuthDone           = 'plOd';
const uint32 kMsgPeerMenu           = 'plPm';
const uint32 kMsgPeerSendPrompt     = 'plPs';
const uint32 kMsgTabChanged         = 'plTc';
const uint32 kMsgNextTab            = 'plN>';
const uint32 kMsgPrevTab            = 'plN<';
const uint32 kMsgTabCtxClose        = 'tCtC';
const uint32 kMsgTabCtxCloseOthers  = 'tCtO';
const uint32 kMsgTabCtxRename       = 'tCtR';
const uint32 kMsgOpenNewTab         = 'plOT';  // from peer: open a new tab
const uint32 kMsgPlayerPlay        = 'plPP';  // mini player: play/pause
const uint32 kMsgPlayerStop        = 'plPS';  // mini player: stop

// Palette colors are now defined in ChatColors.cpp and declared in
// ChatColors.h. _InitPalette() still modifies them via the extern refs.

// Create a Provider instance from config. Handles Anthropic (with OAuth)
// and OpenAI-compat providers. Returns nullptr on failure.
// Provider factory and model heuristics are now in config.cpp:
//   pluto::CreateProvider(cfg)
//   pluto::pluto::GuessMaxOutputTokens(model)
//   pluto::pluto::GuessContextWindow(model)
// Thin wrappers for call sites that don't use the pluto:: prefix.
static std::unique_ptr<Provider> MakeProviderForConfig(
	const pluto::Config& cfg)
{
	return pluto::CreateProvider(cfg);
}

// Derive a user-friendly provider label from config fields.
std::string friendly_provider(const pluto::Config& cfg) {
	if (cfg.provider == "anthropic") return "Anthropic";
	const std::string& u = cfg.base_url;
	if (u.find("api.openai.com")  != std::string::npos) return "OpenAI";
	if (u.find("localhost:11434") != std::string::npos) return "Ollama";
	if (u.find("localhost:1234")  != std::string::npos) return "LM Studio";
	if (u.find("x.ai")           != std::string::npos) return "Grok (xAI)";
	if (u.find("openrouter.ai")  != std::string::npos) return "OpenRouter";
	if (u.find("generativelanguage.googleapis") != std::string::npos)
		return "Google Gemini";
	if (u.find("groq.com")       != std::string::npos) return "Groq";
	if (u.find("deepseek.com")   != std::string::npos) return "DeepSeek";
	if (!u.empty()) return u;
	return "OpenAI-compat";
}

// Color scheme override: "" or "auto" = follow system, "dark", "light".
// Set from Config::color_scheme by _InitPalette.
std::string gColorScheme;

bool is_dark_theme() {
	if (gColorScheme == "dark")  return true;
	if (gColorScheme == "light") return false;
	rgb_color bg = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	return (bg.red + bg.green + bg.blue) < 384;
}

rgb_color brighten(rgb_color c, int by) {
	auto clamp = [](int v) -> uint8 {
		return static_cast<uint8>(std::max(0, std::min(255, v)));
	};
	return rgb_color{ clamp(c.red + by), clamp(c.green + by),
	                  clamp(c.blue + by), c.alpha };
}



std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
	while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
	return s.substr(a, b - a);
}

// Slash-command completion entry: /name — description [group].
struct SlashEntry {
	std::string slash;   // e.g. "/routine daily-summary"
	std::string label;   // display string for the list
	std::string group;   // "Commands", "Routines", "Custom"
};

// Two-line list item for the completion popup: bold name + dim description.
class SlashListItem : public BListItem {
public:
	SlashListItem(const SlashEntry& e)
		: BListItem(), fEntry(e) {}

	void DrawItem(BView* owner, BRect frame, bool /*complete*/) override {
		rgb_color bg = IsSelected()
			? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR)
			: ui_color(B_LIST_BACKGROUND_COLOR);
		owner->SetLowColor(bg);
		owner->SetHighColor(bg);
		owner->FillRect(frame);

		rgb_color textCol = IsSelected()
			? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
			: ui_color(B_LIST_ITEM_TEXT_COLOR);
		bool dark = (bg.red + bg.green + bg.blue) < 384;
		rgb_color dimCol = dark
			? (rgb_color){ 160, 160, 160, 255 }
			: (rgb_color){ 100, 100, 100, 255 };

		BFont bold = *be_plain_font;
		bold.SetFace(B_BOLD_FACE);
		owner->SetFont(&bold);
		owner->SetHighColor(textCol);
		owner->DrawString(fEntry.slash.c_str(),
			BPoint(frame.left + 6, frame.top + bold.Size() + 2));

		BFont small = *be_plain_font;
		small.SetSize(small.Size() - 1);
		owner->SetFont(&small);
		owner->SetHighColor(dimCol);

		// Extract description after the slash name.
		size_t dash = fEntry.label.find(" — ");
		if (dash != std::string::npos) {
			std::string desc = fEntry.label.substr(dash + 5);
			owner->DrawString(desc.c_str(),
				BPoint(frame.left + 6, frame.top + bold.Size() + small.Size() + 5));
		}
	}

	void Update(BView* owner, const BFont* font) override {
		BListItem::Update(owner, font);
		SetHeight(font->Size() * 2 + 10);
	}

	const SlashEntry& Entry() const { return fEntry; }

private:
	SlashEntry fEntry;
};

// Floating completion popup that appears when typing '/' in the input.
class SlashPopup : public BWindow {
public:
	SlashPopup()
		: BWindow(BRect(0, 0, 320, 200), "slash-popup",
		          B_BORDERED_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		          B_NOT_MOVABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
		          | B_NOT_RESIZABLE | B_AVOID_FOCUS | B_WILL_ACCEPT_FIRST_CLICK) {
		fList = new BListView("slashList");
		fList->SetSelectionMessage(new BMessage('slSe'));
		fList->SetInvocationMessage(new BMessage('slIn'));
		auto* scroll = new BScrollView("slashScroll", fList,
			0, false, true, B_NO_BORDER);
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(scroll);
	}

	void SetEntries(const std::vector<SlashEntry>& entries) {
		fAll = entries;
	}

	void Filter(const std::string& prefix) {
		BAutolock locker(this);
		if (!locker.IsLocked()) return;

		while (fList->CountItems() > 0)
			delete fList->RemoveItem((int32)0);

		std::string prev_group;
		for (const auto& e : fAll) {
			// Match prefix case-insensitively.
			if (e.slash.size() < prefix.size()) continue;
			std::string es = e.slash.substr(0, prefix.size());
			std::string ps = prefix;
			for (auto& c : es) c = std::tolower(c);
			for (auto& c : ps) c = std::tolower(c);
			if (es != ps) continue;

			// Group separator.
			if (e.group != prev_group) {
				auto* sep = new BStringItem(("— " + e.group + " —").c_str());
				sep->SetEnabled(false);
				fList->AddItem(sep);
				prev_group = e.group;
			}
			fList->AddItem(new SlashListItem(e));
		}

		if (fList->CountItems() == 0) {
			if (!IsHidden()) Hide();
			return;  // BAutolock releases on return
		}

		// Select first selectable item.
		for (int32 i = 0; i < fList->CountItems(); ++i) {
			if (fList->ItemAt(i)->IsEnabled()) {
				fList->Select(i);
				break;
			}
		}

		// Resize to fit content.
		float h = 0;
		for (int32 i = 0; i < fList->CountItems(); ++i)
			h += fList->ItemAt(i)->Height();
		if (h > 300) h = 300;
		ResizeTo(Bounds().Width(), h + 4);

		if (IsHidden()) Show();
	}

	// Move selection up/down, skipping disabled separators.
	void SelectPrev() {
		BAutolock locker(this);
		if (!locker.IsLocked()) return;
		int32 sel = fList->CurrentSelection();
		for (int32 i = sel - 1; i >= 0; --i) {
			if (fList->ItemAt(i)->IsEnabled()) {
				fList->Select(i);
				fList->ScrollToSelection();
				return;
			}
		}
	}

	void SelectNext() {
		BAutolock locker(this);
		if (!locker.IsLocked()) return;
		int32 sel = fList->CurrentSelection();
		for (int32 i = sel + 1; i < fList->CountItems(); ++i) {
			if (fList->ItemAt(i)->IsEnabled()) {
				fList->Select(i);
				fList->ScrollToSelection();
				return;
			}
		}
	}

	// Get the currently selected slash string, or empty.
	std::string Selected() {
		if (!LockLooper()) return {};
		int32 sel = fList->CurrentSelection();
		if (sel < 0) { UnlockLooper(); return {}; }
		auto* item = dynamic_cast<SlashListItem*>(fList->ItemAt(sel));
		std::string result = item ? item->Entry().slash : std::string();
		UnlockLooper();
		return result;
	}

	BListView* List() { return fList; }

private:
	BListView* fList;
	std::vector<SlashEntry> fAll;
};

// Multiline input: Enter sends, Shift+Enter inserts newline.
// Shows a slash-command completion popup when '/' is typed.
class ChatInput : public BTextView {
public:
	ChatInput() : BTextView("input") {
		SetWordWrap(true);
		SetStylable(false);
		SetInsets(6, 6, 6, 6);
		BFont f = *be_fixed_font;
		SetFontAndColor(&f);
	}

	~ChatInput() override {
		if (fPopup && fPopup->LockLooper()) {
			fPopup->Quit();  // Quit() unlocks and deletes the window
			fPopup = nullptr;
		}
	}

	void SetSlashEntries(const std::vector<SlashEntry>& entries) {
		fEntries = entries;
	}

	void KeyDown(const char* bytes, int32 numBytes) override {
		if (numBytes == 1 && bytes[0] == B_RETURN) {
			int32 mods = 0;
			if (Window() && Window()->CurrentMessage())
				Window()->CurrentMessage()->FindInt32("modifiers", &mods);
			if (!(mods & B_SHIFT_KEY)) {
				// Accept completion if popup is visible, or send.
				if (_PopupVisible()) {
					_AcceptCompletion();
					return;
				}
				if (Window()) Window()->PostMessage(kMsgSend);
				return;
			}
		}

		// ESC cancels the running task.
		if (numBytes == 1 && bytes[0] == B_ESCAPE) {
			if (_PopupVisible()) {
				_HidePopup();
				return;
			}
			// Cancel in-flight request.
			if (Window()) Window()->PostMessage(kMsgStop);
			return;
		}

		// Arrow keys when popup is visible.
		if (numBytes == 1 && _PopupVisible()) {
			if (bytes[0] == B_UP_ARROW) {
				fPopup->SelectPrev();
				return;
			}
			if (bytes[0] == B_DOWN_ARROW) {
				fPopup->SelectNext();
				return;
			}
			if (bytes[0] == B_TAB) {
				_AcceptCompletion();
				return;
			}
		}

		// Arrow up/down for prompt history (when popup is NOT visible).
		if (numBytes == 1 && !_PopupVisible()) {
			if (bytes[0] == B_UP_ARROW) {
				_HistoryUp();
				return;
			}
			if (bytes[0] == B_DOWN_ARROW) {
				_HistoryDown();
				return;
			}
		}

		BTextView::KeyDown(bytes, numBytes);

		// After the key is processed, check if we should show/update popup.
		_UpdatePopup();
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
		// Popup item invoked by double-click.
		if (msg->what == 'slIn') {
			_AcceptCompletion();
			return;
		}
		BTextView::MessageReceived(msg);
	}

	void MakeFocus(bool focus) override {
		BTextView::MakeFocus(focus);
		if (!focus) _HidePopup();
	}

private:
	bool _PopupVisible() const {
		if (!fPopup) return false;
		BAutolock locker(fPopup);
		if (!locker.IsLocked()) return false;
		return !fPopup->IsHidden();
	}

	void _HidePopup() {
		if (!fPopup) return;
		BAutolock locker(fPopup);
		if (!locker.IsLocked()) return;
		if (!fPopup->IsHidden())
			fPopup->Hide();
	}

	void _EnsurePopup() {
		if (!fPopup) {
			fPopup = new SlashPopup();
			fPopup->SetEntries(fEntries);
			// Set the invocation message target to us.
			fPopup->List()->SetTarget(this);
		}
	}

	void _UpdatePopup() {
		std::string text = Text() ? Text() : "";
		// Only trigger on lines starting with '/'.
		if (text.empty() || text[0] != '/' || text.find('\n') != std::string::npos) {
			_HidePopup();
			return;
		}
		// Don't show popup if user already typed a complete command with args.
		// But do show for partial matches.
		std::string prefix = text;
		// If it has a space and matches a known command exactly, hide.
		size_t sp = prefix.find(' ');
		if (sp != std::string::npos) {
			std::string cmd = prefix.substr(0, sp);
			for (const auto& e : fEntries) {
				if (e.slash == cmd) {
					_HidePopup();
					return;
				}
			}
		}

		_EnsurePopup();
		fPopup->Filter(prefix);

		if (fPopup->IsHidden()) return;

		// Position popup above the input field.
		BPoint pt = ConvertToScreen(BPoint(0, 0));
		float ph = fPopup->Bounds().Height();
		fPopup->MoveTo(pt.x, pt.y - ph - 4);
		fPopup->ResizeTo(Bounds().Width(), ph);
	}

	void _AcceptCompletion() {
		if (!fPopup) return;
		std::string slash = fPopup->Selected();
		_HidePopup();
		if (slash.empty()) return;
		// Set the text to the selected slash + trailing space.
		SetText((slash + " ").c_str());
		Select(TextLength(), TextLength());
	}

public:
	// Prompt history (public so MainWindow can call it).
	void AddToHistory(const std::string& text) {
		if (text.empty()) return;
		if (!fHistory.empty() && fHistory.back() == text) return;
		fHistory.push_back(text);
		if (fHistory.size() > 500)
			fHistory.erase(fHistory.begin());
		fHistoryPos = (int)fHistory.size();
		fHistorySaved.clear();
		_SaveHistory();
	}

	void LoadHistory() {
		std::string path = _HistoryPath();
		std::ifstream f(path);
		if (!f) return;
		fHistory.clear();
		std::string line;
		while (std::getline(f, line)) {
			if (!line.empty())
				fHistory.push_back(line);
		}
		if (fHistory.size() > 500)
			fHistory.erase(fHistory.begin(),
				fHistory.begin() + ((int)fHistory.size() - 500));
		fHistoryPos = (int)fHistory.size();
	}

private:
	static std::string _HistoryPath() {
		return SettingsDir() + "/gui-history";
	}

	void _SaveHistory() {
		std::string path = _HistoryPath();
		std::ofstream f(path, std::ios::trunc);
		if (!f) return;
		// Save last 500 entries.
		size_t start = fHistory.size() > 500 ? fHistory.size() - 500 : 0;
		for (size_t i = start; i < fHistory.size(); ++i)
			f << fHistory[i] << "\n";
	}

	void _HistoryUp() {
		if (fHistory.empty()) return;
		if (fHistoryPos == (int)fHistory.size()) {
			// Save current input before navigating.
			fHistorySaved = Text() ? Text() : "";
		}
		if (fHistoryPos > 0) {
			--fHistoryPos;
			SetText(fHistory[fHistoryPos].c_str());
			Select(TextLength(), TextLength());
		}
	}

	void _HistoryDown() {
		if (fHistoryPos >= (int)fHistory.size()) return;
		++fHistoryPos;
		if (fHistoryPos >= (int)fHistory.size()) {
			// Restore saved input.
			SetText(fHistorySaved.c_str());
			Select(TextLength(), TextLength());
		} else {
			SetText(fHistory[fHistoryPos].c_str());
			Select(TextLength(), TextLength());
		}
	}

	SlashPopup* fPopup = nullptr;
	std::vector<SlashEntry> fEntries;
	std::vector<std::string> fHistory;
	int fHistoryPos = 0;
	std::string fHistorySaved;
};


// Side panel text view with clickable links.
class ClickableTextView : public BTextView {
public:
	ClickableTextView(const char* name, uint32 click_msg)
		: BTextView(name), fClickMsg(click_msg) {
		MakeEditable(false);
		SetStylable(true);
		SetWordWrap(true);
		SetInsets(8, 8, 8, 8);
	}

	void MouseDown(BPoint p) override {
		int32 offset = OffsetAt(p);
		BMessage m(fClickMsg);
		m.AddInt32("offset", offset);
		if (Window()) Window()->PostMessage(&m);
		BTextView::MouseDown(p);
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == B_SIMPLE_DATA || msg->what == B_REFS_RECEIVED) {
			// Forward file drops to the main window.
			if (Window()) {
				BMessage forward(*msg);
				forward.what = B_REFS_RECEIVED;
				Window()->PostMessage(&forward);
			}
			return;
		}
		BTextView::MessageReceived(msg);
	}

private:
	uint32 fClickMsg;
};

// Clickable status bar — posts kMsgModelQuickSwitch on click.
class ClickableStatus : public BStringView {
public:
	ClickableStatus(const char* name, const char* text)
		: BStringView(name, text) {}

	void MouseDown(BPoint where) override {
		if (Window())
			Window()->PostMessage(kMsgModelQuickSwitch);
		BStringView::MouseDown(where);
	}
};

}  // namespace

// Context-window usage gauge.
class TokenGauge : public BView {
public:
	TokenGauge()
		: BView("tokenGauge", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE) {
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 8));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 8));
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	void SetValues(long used, long max) {
		fUsed = used < 0 ? 0 : used;
		fMax = max < 1 ? 1 : max;
		Invalidate();
	}

	void Draw(BRect /*updateRect*/) override {
		BRect r = Bounds();
		if (r.Width() < 4 || r.Height() < 4) return;

		// Background track - subtle
		rgb_color track = gColorScheme == "dark" 
			? (rgb_color){ 55, 65, 81, 255 }   // #374151
			: (rgb_color){ 229, 231, 235, 255 }; // #E5E7EB
		SetHighColor(track);
		FillRect(r);

		float frac = std::min(1.0f,
			static_cast<float>(fUsed) / static_cast<float>(fMax));
		if (frac > 0.0f) {
			BRect fill = r;
			fill.right = fill.left + r.Width() * frac;
			// Modern gradient-like colors
			rgb_color c;
			if (frac < 0.50f) {
				c = gColorScheme == "dark" 
					? (rgb_color){  34, 197,  94, 255 }  // #22C55E - green
					: (rgb_color){  22, 163,  74, 255 };   // #16A34A
			} else if (frac < 0.75f) {
				c = gColorScheme == "dark"
					? (rgb_color){ 250, 204,  21, 255 }  // #FACC15 - amber
					: (rgb_color){ 217, 119,   6, 255 };   // #D97706
			} else if (frac < 0.90f) {
				c = gColorScheme == "dark"
					? (rgb_color){ 249, 115,  22, 255 }  // #F97316 - orange
					: (rgb_color){ 234,  88,  12, 255 };   // #EA580C
			} else {
				c = gColorScheme == "dark"
					? (rgb_color){ 239,  68,  68, 255 }  // #EF4444 - red
					: (rgb_color){ 220,  38,  38, 255 };   // #DC2626
			}
			SetHighColor(c);
			FillRect(fill);
		}

		// Subtle border
		rgb_color border = gColorScheme == "dark" 
			? (rgb_color){ 75, 85, 99, 255 }   // #4B5563
			: (rgb_color){ 209, 213, 219, 255 }; // #D1D5DB
		SetHighColor(border);
		StrokeRect(r);
	}

private:
	long fUsed = 0;
	long fMax  = 1;
};

// Image preview for the side panel.
class ImagePreviewView : public BView {
public:
	ImagePreviewView()
		: BView("imagePreview", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE) {
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	}

	~ImagePreviewView() override { delete fBitmap; }

	void SetBitmap(BBitmap* bitmap) {
		delete fBitmap;
		fBitmap = bitmap;
		if (fBitmap) {
			BRect br = fBitmap->Bounds();
			fImgWidth = br.Width() + 1;
			fImgHeight = br.Height() + 1;
			// Set height to maintain aspect ratio at current width.
			float w = Bounds().Width();
			if (w < 10) w = 200;
			float h = fImgHeight * (w / fImgWidth);
			if (h > 400) h = 400;
			SetExplicitMinSize(BSize(B_SIZE_UNSET, h));
			SetExplicitPreferredSize(BSize(B_SIZE_UNSET, h));
		} else {
			SetExplicitMinSize(BSize(B_SIZE_UNSET, 0));
			SetExplicitPreferredSize(BSize(B_SIZE_UNSET, 0));
		}
		InvalidateLayout();
		Invalidate();
	}

	bool HasBitmap() const { return fBitmap != nullptr; }
	BBitmap* Bitmap() const { return fBitmap; }

	void MouseDown(BPoint where) override {
		if (!fBitmap) return;
		uint32 buttons = 0;
		GetMouse(&where, &buttons);
		if (buttons & B_SECONDARY_MOUSE_BUTTON) {
			_ShowContextMenu(where);
		}
	}

	void MessageReceived(BMessage* msg) override {
		if (msg->what == 'plIS') {
			// Forward save request to the window (BFilePanel needs a BLooper).
			Window()->PostMessage('plIS');
		} else {
			BView::MessageReceived(msg);
		}
	}

	void Draw(BRect /*update*/) override {
		if (!fBitmap) return;
		BRect bounds = Bounds();
		// Scale to fit width, maintain aspect ratio.
		float scale = bounds.Width() / fImgWidth;
		float h = fImgHeight * scale;
		if (h > bounds.Height()) {
			h = bounds.Height();
			scale = h / fImgHeight;
		}
		float w = fImgWidth * scale;
		float x = (bounds.Width() - w) / 2;
		BRect dest(x, 0, x + w, h);
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fBitmap, fBitmap->Bounds(), dest);
	}

private:
	void _ShowContextMenu(BPoint where) {
		BPopUpMenu* menu = new BPopUpMenu("image_ctx", false, false);
		menu->AddItem(new BMenuItem("Save image" B_UTF8_ELLIPSIS,
			new BMessage('plIS')));
		menu->SetTargetForItems(this);
		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
	}

	BBitmap* fBitmap = nullptr;
	float fImgWidth = 0;
	float fImgHeight = 0;
};

// ----------------------------------------------------------------
// Model picker: a small window with search + scrollable list.
// Fetches models in a background thread, filters as the user types.
// ----------------------------------------------------------------

class ModelPickerWindow : public BWindow {
public:
	ModelPickerWindow(BMessenger target, const Config* cfg)
		: BWindow(BRect(0, 0, 800, 400), "Select model",
		          B_FLOATING_WINDOW,
		          B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
		              | B_CLOSE_ON_ESCAPE),
		  fTarget(target), fConfig(cfg)
	{
		fFilter = new BTextControl("filter", "Search:", "",
			new BMessage('filt'));
		fFilter->SetModificationMessage(new BMessage('filt'));

		fColumnList = new BColumnListView("models", 0, B_FANCY_BORDER);
		fColumnList->AddColumn(
			new BStringColumn("Provider", 140, 80, 300, B_TRUNCATE_END), 0);
		fColumnList->AddColumn(
			new BStringColumn("Model", 400, 150, 800, B_TRUNCATE_END), 1);
		fColumnList->SetInvocationMessage(new BMessage('pick'));
		fColumnList->SetSortingEnabled(true);
		fColumnList->SetSortColumn(fColumnList->ColumnAt(0), true, true);

		fStatus = new BStringView("status", "Fetching models…");

		fColumnList->SetExplicitMinSize(BSize(500, 300));

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fFilter)
			.Add(fColumnList, 1.0)
			.Add(fStatus);

		CenterOnScreen();
		fFilter->MakeFocus(true);

		// Start fetching models in a background thread.
		thread_id tid = spawn_thread(_FetchThread, "fetch_models",
			B_NORMAL_PRIORITY, this);
		if (tid >= 0) resume_thread(tid);
	}

	void MessageReceived(BMessage* msg) override {
		switch (msg->what) {
			case 'filt':
				_ApplyFilter();
				break;

			case 'pick': {
				BRow* row = fColumnList->CurrentSelection();
				if (!row) break;
				auto* field = dynamic_cast<BStringField*>(row->GetField(1));
				if (!field) break;
				std::string display = field->String();
				std::string id;
				for (const auto& m : fAllModels) {
					if (m.display == display) { id = m.id; break; }
				}
				if (id.empty()) id = display;

				BMessage pick(kMsgModelQuickSwitch);
				pick.AddString("model", id.c_str());
				fTarget.SendMessage(&pick);
				Quit();
				break;
			}

			case 'done':
				_OnFetchDone(msg);
				break;

			default:
				BWindow::MessageReceived(msg);
		}
	}

private:
	struct ModelEntry {
		std::string id;
		std::string display;   // what the user sees
		std::string provider;  // "Anthropic", "OpenAI", "Ollama", etc.
	};

	void _ApplyFilter() {
		std::string query = fFilter->Text() ? fFilter->Text() : "";
		for (auto& c : query)
			c = std::tolower(static_cast<unsigned char>(c));

		BRow* r;
		while ((r = fColumnList->RowAt((int32)0)) != nullptr) {
			fColumnList->RemoveRow(r);
			delete r;
		}

		int shown = 0;
		for (const auto& m : fAllModels) {
			if (!query.empty()) {
				std::string lower = m.display + " " + m.provider;
				for (auto& c : lower)
					c = std::tolower(static_cast<unsigned char>(c));
				if (lower.find(query) == std::string::npos)
					continue;
			}
			BRow* row = new BRow();
			row->SetField(new BStringField(m.provider.c_str()), 0);
			row->SetField(new BStringField(m.display.c_str()), 1);
			fColumnList->AddRow(row);
			++shown;
		}

		BString status;
		status.SetToFormat("%d / %ld models",
			shown, (long)fAllModels.size());
		fStatus->SetText(status.String());
	}

	void _OnFetchDone(BMessage* msg) {
		int32 count = 0;
		const char* id;
		while (msg->FindString("id", count, &id) == B_OK) {
			const char* display = "";
			const char* provider = "";
			msg->FindString("display", count, &display);
			msg->FindString("provider", count, &provider);
			ModelEntry e;
			e.id = id;
			e.display = (display && *display) ? display : id;
			e.provider = (provider && *provider) ? provider : "?";
			fAllModels.push_back(std::move(e));
			++count;
		}
		// Sort by provider then display name.
		std::sort(fAllModels.begin(), fAllModels.end(),
			[](const ModelEntry& a, const ModelEntry& b) {
				if (a.provider != b.provider) return a.provider < b.provider;
				return a.display < b.display;
			});
		_ApplyFilter();
	}

	static int32 _FetchThread(void* data) {
		auto* self = static_cast<ModelPickerWindow*>(data);
		const Config* cfg = self->fConfig;
		BMessage done('done');

		auto fetch = [](const std::string& url,
		                const char* arr_key, const char* id_key,
		                const char* label_key,
		                const std::vector<std::string>& hdrs_vec,
		                const char* provider_name,
		                BMessage& out)
		{
			CURL* c = curl_easy_init();
			if (!c) return;
			std::string body;
			auto cb = [](char* p, size_t s, size_t n, void* u) -> size_t {
				static_cast<std::string*>(u)->append(p, s * n);
				return s * n;
			};
			struct curl_slist* hdrs = nullptr;
			for (const auto& h : hdrs_vec)
				hdrs = curl_slist_append(hdrs, h.c_str());
			curl_easy_setopt(c, CURLOPT_URL, url.c_str());
			if (hdrs) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
			curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,
				static_cast<size_t(*)(char*,size_t,size_t,void*)>(cb));
			curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
			curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
			curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
			// Enable SSL verification for HTTPS URLs
			if (url.rfind("https://", 0) == 0) {
				curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
				curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
				curl_easy_setopt(c, CURLOPT_CAINFO,
					"/system/data/ssl/CARootCertificates.pem");
			}
			CURLcode rc = curl_easy_perform(c);
			long http = 0;
			curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
			curl_slist_free_all(hdrs);
			curl_easy_cleanup(c);
			if (rc != CURLE_OK || http != 200) return;
			try {
				json::Value root = json::Value::parse(body);
				const json::Value* arr = root.find(arr_key);
				if (!arr || !arr->is_arr()) return;
				for (const auto& m : arr->as_arr()) {
					std::string mid = m.str(id_key);
					if (mid.empty()) continue;
					if (mid.rfind("models/", 0) == 0)
						mid = mid.substr(7);
					std::string mlabel = m.str(label_key);
					if (mlabel.empty()) mlabel = mid;
					if (mlabel.rfind("models/", 0) == 0)
						mlabel = mlabel.substr(7);
					out.AddString("id", mid.c_str());
					out.AddString("display", mlabel.c_str());
					out.AddString("provider", provider_name);
				}
			} catch (const std::exception&) {}
		};

		// Helper: try to resolve an API key by env var name or key file.
		auto try_key = [&cfg](const std::string& env_name) -> std::string {
			if (env_name.empty()) return "";
			const char* v = std::getenv(env_name.c_str());
			if (v && *v) return v;
			std::string key = pluto::ResolveProviderKey(env_name);
			if (!key.empty()) return key;
			// Fallback: if this is the current provider's key, try ResolveApiKey
			// which also checks the legacy key file.
			if (env_name == cfg->api_key_env)
				return pluto::ResolveApiKey(*cfg);
			return {};
		};

		// --- Anthropic ---
		{
			pluto::OAuthTokens tokens = pluto::LoadOAuthTokens();
			std::string akey;
			bool use_oauth = false;
			if (!tokens.empty()) {
				akey = pluto::OAuthEnsureValidToken(tokens);
				if (!akey.empty()) use_oauth = true;
			}
			if (akey.empty())
				akey = try_key("ANTHROPIC_API_KEY");

			if (!akey.empty()) {
				std::vector<std::string> hdrs;
				if (use_oauth) {
					hdrs.push_back("Authorization: Bearer " + akey);
					hdrs.push_back("anthropic-beta: oauth-2025-04-20");
				} else {
					hdrs.push_back("x-api-key: " + akey);
				}
				hdrs.push_back("anthropic-version: 2023-06-01");
				fetch("https://api.anthropic.com/v1/models",
					"data", "id", "display_name", hdrs, "Anthropic", done);
			}
		}

		// --- OpenAI ---
		{
			std::string key = try_key("OPENAI_API_KEY");
			if (!key.empty()) {
				std::vector<std::string> hdrs;
				hdrs.push_back("Authorization: Bearer " + key);
				fetch("https://api.openai.com/v1/models",
					"data", "id", "id", hdrs, "OpenAI", done);
			}
		}

		// --- Google Gemini ---
		{
			std::string key = try_key("GEMINI_API_KEY");
			if (!key.empty()) {
				std::string url =
					"https://generativelanguage.googleapis.com/v1beta/models?key=" + key;
				fetch(url, "models", "name", "displayName", {}, "Gemini", done);
			}
		}

		// --- xAI (Grok) ---
		{
			std::string key = try_key("XAI_API_KEY");
			if (!key.empty()) {
				std::vector<std::string> hdrs;
				hdrs.push_back("Authorization: Bearer " + key);
				fetch("https://api.x.ai/v1/models",
					"data", "id", "id", hdrs, "xAI", done);
			}
		}

		// --- OpenRouter ---
		{
			std::string key = try_key("OPENROUTER_API_KEY");
			if (!key.empty()) {
				std::vector<std::string> hdrs;
				hdrs.push_back("Authorization: Bearer " + key);
				fetch("https://openrouter.ai/api/v1/models",
					"data", "id", "id", hdrs, "OpenRouter", done);
			}
		}

		// --- Ollama (local) ---
		{
			fetch("http://localhost:11434/api/tags",
				"models", "name", "name", {}, "Ollama", done);
		}

		// --- Current base_url (if different from the above) ---
		if (!cfg->base_url.empty()) {
			std::string base = cfg->base_url;
			while (!base.empty() && base.back() == '/') base.pop_back();

			// Skip if it's one of the providers already fetched above.
			bool skip = (base.find("api.openai.com") != std::string::npos
			          || base.find("api.anthropic.com") != std::string::npos
			          || base.find("generativelanguage.googleapis.com") != std::string::npos
			          || base.find("api.x.ai") != std::string::npos
			          || base.find("openrouter.ai") != std::string::npos
			          || base.find("localhost:11434") != std::string::npos);

			if (!skip) {
				std::vector<std::string> chdrs;
				std::string ckey = pluto::ResolveApiKey(*cfg);
				if (!ckey.empty())
					chdrs.push_back("Authorization: Bearer " + ckey);

				const char* prov = "Custom";
				if (base.find("openrouter") != std::string::npos)
					prov = "OpenRouter";
				else if (base.find("groq") != std::string::npos)
					prov = "Groq";
				else if (base.find("together") != std::string::npos)
					prov = "Together";
				else if (base.find("mistral") != std::string::npos)
					prov = "Mistral";

				fetch(base + "/v1/models", "data", "id", "id",
					chdrs, prov, done);
			}
		}

		BMessenger(self).SendMessage(&done);
		return 0;
	}

	BMessenger          fTarget;
	const Config*       fConfig;
	BTextControl*       fFilter      = nullptr;
	BColumnListView*    fColumnList  = nullptr;
	BStringView*        fStatus      = nullptr;
	std::vector<ModelEntry> fAllModels;
};

// BTabView subclass that notifies the window on tab selection change.
class PlutoTabView : public BTabView {
public:
	PlutoTabView(const char* name)
		: BTabView(name, B_WIDTH_FROM_LABEL) {
		SetBorder(B_NO_BORDER);
	}

	void Select(int32 index) override {
		BTabView::Select(index);
		if (Window())
			Window()->PostMessage(kMsgTabChanged);
	}

	void MouseDown(BPoint where) override {
		int32 buttons = 0;
		GetMouse(&where, (uint32*)&buttons, false);
		if (buttons & B_SECONDARY_MOUSE_BUTTON) {
			// Find which tab was right-clicked by checking tab frames.
			int32 clicked = -1;
			for (int32 i = 0; i < CountTabs(); ++i) {
				BRect frame = TabFrame(i);
				if (frame.Contains(where)) { clicked = i; break; }
			}
			if (clicked < 0) { BTabView::MouseDown(where); return; }

			// Store the clicked index for the message handler.
			BPopUpMenu* menu = new BPopUpMenu("tab_ctx", false, false);
			BMessage* msgClose = new BMessage(kMsgTabCtxClose);
			msgClose->AddInt32("tab_index", clicked);
			menu->AddItem(new BMenuItem("Close", msgClose));

			if (CountTabs() > 1) {
				BMessage* msgOthers = new BMessage(kMsgTabCtxCloseOthers);
				msgOthers->AddInt32("tab_index", clicked);
				menu->AddItem(new BMenuItem("Close others", msgOthers));
			}

			menu->AddSeparatorItem();
			BMessage* msgRename = new BMessage(kMsgTabCtxRename);
			msgRename->AddInt32("tab_index", clicked);
			menu->AddItem(new BMenuItem("Rename" B_UTF8_ELLIPSIS, msgRename));

			menu->SetTargetForItems(Window());
			ConvertToScreen(&where);
			menu->Go(where, true, true, true);
			return;
		}
		BTabView::MouseDown(where);
	}
};

// ----------------------------------------------------------------

MainWindow::MainWindow(Provider* provider, Config* cfg, ToolRegistry* tools,
                       const std::vector<std::shared_ptr<McpClient>>& mcp_clients)
	: BWindow(BRect(0, 0, 1020, 720), "Pluto", B_TITLED_WINDOW,
	          B_ASYNCHRONOUS_CONTROLS),
	  fProvider(provider), fConfig(cfg), fTools(tools),
	  fMcpClients(mcp_clients),
	  fFontDelta(cfg->font_delta) {

	_UpdateTitle();

	_BuildMenuBar();
	_InitPalette();

	// Side panel (must be created before _RestoreWorkspace which may
	// call OnImage → _LoadImagePreviewFromBase64 → fImagePreview->SetBitmap)
	fSideTitle = new BStringView("sideTitle", "Detail panel");
	BFont titleFont = *be_plain_font;
	titleFont.SetFace(B_BOLD_FACE);
	fSideTitle->SetFont(&titleFont);

	fSideView = new ClickableTextView("sideView", kMsgSideLink);
	fSideView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fSideView->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fSideScroll = new BScrollView("sideScroll", fSideView,
	                              0, false, true, B_FANCY_BORDER);
	fSideScroll->SetExplicitMinSize(BSize(140, B_SIZE_UNSET));

	fImagePreview = new ImagePreviewView();
	fImagePreview->Hide();

	// Mini audio player bar (hidden by default).
	fPlayerBar = new BGroupView(B_HORIZONTAL, B_USE_SMALL_SPACING);
	fPlayerPlay = new BButton("\xe2\x96\xb6",
		new BMessage(kMsgPlayerPlay));   // ▶
	fPlayerStop = new BButton("\xe2\x96\xa0",
		new BMessage(kMsgPlayerStop));   // ■
	fPlayerTime = new BStringView("time", "0:00");
	fPlayerPlay->SetExplicitSize(BSize(28, 24));
	fPlayerStop->SetExplicitSize(BSize(28, 24));
	fPlayerBar->GroupLayout()->AddView(fPlayerPlay);
	fPlayerBar->GroupLayout()->AddView(fPlayerStop);
	fPlayerBar->GroupLayout()->AddView(fPlayerTime);
	fPlayerBar->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
	fPlayerBar->Hide();

	// Tab view: each tab holds a ConversationView.
	fTabView = new PlutoTabView("tabs");
	if (!_RestoreWorkspace())
		_CreateTab();
	fPrevConv = _ActiveConv();

	auto* sideGroup = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	sideGroup->GroupLayout()->AddView(fSideTitle);
	sideGroup->GroupLayout()->AddView(fPlayerBar);
	sideGroup->GroupLayout()->AddView(fSideScroll);
	sideGroup->GroupLayout()->AddView(fImagePreview);

	// Split view: chat | side
	// Force both children to accept any width so the splitter is fully
	// draggable. Without explicit min+max the layout engine locks them
	// at preferred size and the grip won't move.
	fTabView->SetExplicitMinSize(BSize(100, 100));
	fTabView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	sideGroup->SetExplicitMinSize(BSize(100, 100));
	sideGroup->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	fSideTitle->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fSideScroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	fSplitView = new BSplitView(B_HORIZONTAL, 10);
	fSplitView->AddChild(fTabView, 2.0f);
	fSplitView->AddChild(sideGroup, 1.0f);
	fSplitView->SetCollapsible(0, true);
	fSplitView->SetCollapsible(1, true);

	// Input (multiline)
	auto* chat = new ChatInput();
	chat->LoadHistory();
	fInput = chat;

	// Populate slash-command completion entries.
	{
		std::vector<SlashEntry> entries;
		// Built-in commands.
		entries.push_back({"/reset",    "/reset — start a new conversation", "Commands"});
		entries.push_back({"/clear",    "/clear — start a new conversation", "Commands"});
		entries.push_back({"/help",     "/help — show available commands", "Commands"});
		entries.push_back({"/tools",    "/tools — list registered tools", "Commands"});
		entries.push_back({"/model",    "/model [ID] — show or switch model", "Commands"});
		entries.push_back({"/cost",     "/cost — show session token usage", "Commands"});
		entries.push_back({"/routines", "/routines — list available routines", "Commands"});
		entries.push_back({"/commands", "/commands — list custom slash commands", "Commands"});
		entries.push_back({"/init",     "/init [--force] — create pluto.md project memory", "Commands"});
		entries.push_back({"/plan",     "/plan — toggle plan mode (read-only)", "Commands"});
		entries.push_back({"/mode",     "/mode [name|off] — switch mode", "Commands"});
		entries.push_back({"/modes",    "/modes — list available modes", "Commands"});
		entries.push_back({"/title",    "/title [new] — show or set session title", "Commands"});
		entries.push_back({"/tag",      "/tag [name] — show or add session tag", "Commands"});
		entries.push_back({"/untag",    "/untag <name> — remove a session tag", "Commands"});
		entries.push_back({"/wiki",     "/wiki [url|file] — show wiki or ingest content", "Commands"});
		entries.push_back({"/map",      "/map — scan workspace and build knowledge graph", "Commands"});
		entries.push_back({"/map path", "/map path <from> <to> — find path between nodes", "Commands"});
		entries.push_back({"/map query","/map query <question> — query the graph with LLM", "Commands"});

		// Routines.
		auto routines = pluto::ListRoutines();
		for (const auto& r : routines) {
			std::string slash = "/routine " + r.name;
			std::string label = slash + " — " + r.schedule;
			if (!r.enabled) label += " (paused)";
			entries.push_back({slash, label, "Routines"});
		}

		// Custom commands.
		for (const auto& c : fCustomCommands) {
			std::string slash = "/" + c.name;
			std::string desc = c.description.empty() ? "custom command" : c.description;
			entries.push_back({slash, slash + " — " + desc, "Custom"});
		}

		chat->SetSlashEntries(entries);
	}

	fInputScroll = new BScrollView("inputScroll", fInput,
	                               0, false, true, B_FANCY_BORDER);
	fInputScroll->SetExplicitMinSize(BSize(200, 68));
	fInputScroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 68));

	fSend = new BButton("send", "Send", new BMessage(kMsgSend));

	// Status + gauge
	fStatus = new ClickableStatus("status", "");
	BFont sf = *be_plain_font;
	sf.SetSize(sf.Size() - 1);
	fStatus->SetFont(&sf);
	fStatus->SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
	fStatus->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fStatus->SetToolTip("Click to switch model");

	fGauge = new TokenGauge();

	// Image chips — hidden by default, shows attached image filenames
	// as individual buttons with X to remove.
	fImageChips = new BGroupView(B_HORIZONTAL, 4);
	fImageChips->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fImageChips->Hide();

	// Find bar — hidden by default, toggled via Cmd+F. Enter = next match.
	fFindBar = new BTextControl("findbar", "Find:", "",
		new BMessage(kMsgFindNext));
	fFindBar->Hide();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fFindBar)
			.Add(fSplitView)
			.Add(fImageChips)
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(fInputScroll)
				.Add(fSend)
			.End()
			.Add(fStatus)
			.Add(fGauge)
		.End();

	SetSizeLimits(640, 32768, 420, 32768);

	// Welcome banner — two-column layout: info left, ASCII art right.
	// All mono so columns align.
	{
		static const char* kArt[] = {
			R"(       _       _        )",
			R"( _ __ | |_   _ | |_  ___)",
			R"(| '_ \| | | | || __|/ _ \)",
			R"(| |_) | | |_| || |_| (_) |)",
			R"(| .__/|_|\__,_| \__|\___/)",
			R"(|_|                      )",
		};
		const int kArtLines = 6;
		const size_t kLeftCol = 42;  // pad info lines to this width

		// Pre-build info values.
		pluto::OAuthTokens check_tokens = pluto::LoadOAuthTokens();
		bool has_oauth = !check_tokens.empty();

		long total_remote = 0;
		for (auto& c : fMcpClients)
			total_remote += (long)c->tools().size();
		long builtin = (long)fTools->list().size() - total_remote;

		std::string mcp_str;
		for (size_t i = 0; i < fMcpClients.size(); ++i) {
			if (i > 0) mcp_str += ", ";
			mcp_str += fMcpClients[i]->server_name();
			char buf[32];
			std::snprintf(buf, sizeof(buf), " (%ld)",
				(long)fMcpClients[i]->tools().size());
			mcp_str += buf;
		}

		auto routines = pluto::ListRoutines();
		std::string ws = pluto::WorkspaceRoot();

		// Helper: pad current line position to kLeftCol.
		auto pad_to = [&](size_t cur_len) {
			if (cur_len < kLeftCol) {
				std::string sp(kLeftCol - cur_len, ' ');
				_Append(sp, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			}
		};

		// Helper: append art for a given line index.
		auto emit_art = [&](int line) {
			if (line < kArtLines) {
				_Append(kArt[line], ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			}
		};

		// Line 0: Title + art[0]
		_Append("  Pluto", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE, true);
		_Append("  Haiku-native AI assistant", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
		pad_to(34);  // "  Pluto" (7) + "  Haiku..." (27) = 34
		emit_art(0);
		_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));

		// Line 1: blank + art[1]
		pad_to(0);
		emit_art(1);
		_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));

		// Line 2: Provider + art[2]
		{
			_Append("  Provider   ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			const Config& bc = _ActiveConv()
				? _ActiveConv()->GetConfig() : *fConfig;
			std::string prov = friendly_provider(bc);
			if (has_oauth && bc.provider == "anthropic")
				prov += " (Claude Max)";
			const size_t kMaxProv = kLeftCol - 14;
			if (prov.size() > kMaxProv)
				prov = prov.substr(0, kMaxProv - 1) + "\xe2\x80\xa6";
			_Append(prov, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			pad_to(13 + prov.size());
			emit_art(2);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}

		// Line 3: Model + art[3]
		{
			_Append("  Model      ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			std::string model = _ActiveConv()
				? _ActiveConv()->GetModel() : fConfig->model;
			const size_t kMaxModel = kLeftCol - 14;  // 13 prefix + 1 space
			if (model.size() > kMaxModel)
				model = model.substr(0, kMaxModel - 1) + "\xe2\x80\xa6";  // …
			_Append(model, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			pad_to(13 + model.size());
			emit_art(3);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}

		// Line 4: Tools + art[4]
		{
			_Append("  Tools      ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%ld built-in", builtin);
			std::string tools_val = buf;
			if (total_remote > 0) {
				std::snprintf(buf, sizeof(buf), " + %ld remote", total_remote);
				tools_val += buf;
			}
			_Append(tools_val, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			pad_to(13 + tools_val.size());
			emit_art(4);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}

		// Line 5: MCP or Routines + art[5]
		if (!mcp_str.empty()) {
			_Append("  MCP        ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append(mcp_str, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			pad_to(13 + mcp_str.size());
			emit_art(5);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		} else {
			pad_to(0);
			emit_art(5);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}

		// Remaining info lines (no art).
		if (!mcp_str.empty() && !routines.empty()) {
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%ld routine%s",
				(long)routines.size(), routines.size() == 1 ? "" : "s");
			_Append("  Routines   ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append(buf, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}
		if (mcp_str.empty() && !routines.empty()) {
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%ld routine%s",
				(long)routines.size(), routines.size() == 1 ? "" : "s");
			_Append("  Routines   ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append(buf, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}
		if (!ws.empty()) {
			_Append("  Workspace  ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append(ws, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
			_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}

		_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		_Append("  Enter", ui_color(B_DOCUMENT_TEXT_COLOR), B_BOLD_FACE, true);
		_Append(" send  ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
		_Append("Shift+Enter", ui_color(B_DOCUMENT_TEXT_COLOR), B_BOLD_FACE, true);
		_Append(" newline  ", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
		_Append("/", ui_color(B_DOCUMENT_TEXT_COLOR), B_BOLD_FACE, true);
		_Append(" commands\n\n", ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
	}

	_ResetSidePanel();

	// Load custom slash commands.
	{
		std::string cmd_dir = SettingsDir() + "/commands";
		DIR* d = opendir(cmd_dir.c_str());
		if (d) {
			struct dirent* e;
			while ((e = readdir(d)) != nullptr) {
				std::string name = e->d_name;
				if (name.size() < 4 || name.substr(name.size() - 3) != ".md")
					continue;
				std::string path = cmd_dir + "/" + name;
				std::ifstream f(path);
				if (!f) continue;
				std::ostringstream os;
				os << f.rdbuf();
				CustomCommand c;
				c.name = name.substr(0, name.size() - 3);
				c.body = os.str();
				// Parse optional frontmatter.
				if (c.body.size() >= 4 && c.body.substr(0, 4) == "---\n") {
					size_t end = c.body.find("\n---\n", 4);
					if (end != std::string::npos) {
						std::string fm = c.body.substr(4, end - 4);
						size_t dp = fm.find("description:");
						if (dp != std::string::npos) {
							size_t nl = fm.find('\n', dp);
							c.description = fm.substr(dp + 12,
								(nl == std::string::npos ? fm.size() : nl) - dp - 12);
							while (!c.description.empty() && c.description.front() == ' ')
								c.description.erase(0, 1);
						}
						c.body = c.body.substr(end + 5);
					}
				}
				fCustomCommands.push_back(std::move(c));
			}
			closedir(d);
		}
	}

	_UpdateStatus();
	_ApplyViewColors();  // apply color scheme override after views exist
	CenterOnScreen();
	fInput->MakeFocus(true);
}

MainWindow::~MainWindow() {
}

// ---- active tab + thin wrappers ----

ConversationView* MainWindow::_ActiveConv() const {
	if (!fTabView || fTabView->CountTabs() == 0) return nullptr;
	return static_cast<ConversationView*>(
		fTabView->ViewForTab(fTabView->Selection()));
}

void MainWindow::_Append(const std::string& text, rgb_color color,
                          uint16 face, bool mono) {
	if (auto* c = _ActiveConv()) c->AppendText(text, color, face, mono);
}

// Legacy _Output() — returns DocView for invalidation.
BView* MainWindow::_Output() const {
	auto* c = _ActiveConv();
	return c ? c->DocView() : nullptr;
}

std::string MainWindow::GetSessionTitle() const {
	return _ActiveConv() ? _ActiveConv()->GetSessionTitle() : std::string();
}

std::string MainWindow::GetActiveModel() const {
	return _ActiveConv() ? _ActiveConv()->GetModel() : fConfig->model;
}

bool MainWindow::IsBusy() const {
	return _ActiveConv() ? _ActiveConv()->IsBusy() : false;
}

std::string MainWindow::SearchContext(const std::string& topic) const {
	if (topic.empty() || !fTabView) return {};

	// Lowercase topic for case-insensitive matching.
	std::string lower_topic = topic;
	for (auto& c : lower_topic)
		c = std::tolower(static_cast<unsigned char>(c));

	std::string result;
	const size_t kMaxLen = 2000;

	for (int32 t = 0; t < fTabView->CountTabs() && result.size() < kMaxLen; ++t) {
		auto* conv = dynamic_cast<ConversationView*>(fTabView->ViewForTab(t));
		if (!conv) continue;
		const auto& msgs = conv->Messages();
		for (const auto& m : msgs) {
			if (result.size() >= kMaxLen) break;
			for (const auto& b : m.content) {
				if (b.type != pluto::ContentBlock::TEXT) continue;
				// Case-insensitive search in block text.
				std::string lower_text = b.text;
				for (auto& c : lower_text)
					c = std::tolower(static_cast<unsigned char>(c));
				if (lower_text.find(lower_topic) == std::string::npos) continue;
				// Found a match — include the block (truncated).
				if (!result.empty()) result += "\n---\n";
				result += "[" + m.role + "] ";
				if (b.text.size() + result.size() > kMaxLen)
					result += b.text.substr(0, kMaxLen - result.size()) + "...";
				else
					result += b.text;
			}
		}
	}
	return result;
}

bool MainWindow::QuitRequested() {
	_SaveWorkspace();
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

// ---- ConversationDelegate implementation ----
// These bridge ConversationView events to MainWindow UI chrome.
// For now they delegate to existing MainWindow methods. In the future
// (R3+) they will forward to the active tab's ConversationView.

void MainWindow::OnStatusChanged() {
	_UpdateStatus();
}

void MainWindow::OnTitleChanged(const std::string& /*title*/) {
	_UpdateTitle();
	// Refresh all tab labels from their ConversationView titles + provider:model.
	if (fTabView) {
		for (int32 i = 0; i < fTabView->CountTabs(); ++i) {
			auto* conv = static_cast<ConversationView*>(
				fTabView->ViewForTab(i));
			if (!conv) continue;
			fTabView->TabAt(i)->SetLabel(_BuildTabLabel(conv).c_str());
		}
		fTabView->Invalidate();
	}
}

void MainWindow::OnError(const std::string& error) {
	// Show native Haiku notification for errors.
	BNotification n(B_ERROR_NOTIFICATION);
	n.SetGroup("Pluto");
	n.SetTitle("Pluto — error");
	std::string text = error;
	if (text.size() > 200)
		text = text.substr(0, 200) + "\xe2\x80\xa6";
	n.SetContent(text.c_str());
	n.SetMessageID("pluto-error");
	n.SetOnClickApp("application/x-vnd.Hay-pluto");
	n.Send(static_cast<bigtime_t>(10) * 1000000LL);

	// Also append to active conversation.
	auto* conv = _ActiveConv();
	if (conv) conv->HandleError(error);
}

void MainWindow::OnToolOutput(const std::string& tool_name,
                               const std::string& body,
                               const std::string& input_json,
                               bool is_error) {
	if (tool_name == "edit_file" && !is_error && !input_json.empty()) {
		_RenderEditDiff(input_json);
	} else {
		_UpdateSidePanel(tool_name, body, is_error);
	}
	if (fImagePreview->HasBitmap() && !fImagePreview->IsHidden()) {
		_Append("\xf0\x9f\x96\xbc [image preview in side panel]\n",
			ui_color(B_DOCUMENT_TEXT_COLOR), B_ITALIC_FACE);
	}
}

void MainWindow::OnImage(const std::string& base64_data,
                          const std::string& /*media_type*/) {
	_LoadImagePreviewFromBase64(base64_data);
}

void MainWindow::OnTurnDone(const std::string& preview) {
	_SetSendStopMode(false);
	fInput->MakeEditable(true);
	fInput->MakeFocus(true);
	_UpdateTitle();
	_UpdateStatus();
	_UpdateMiniMap();

	// If /init was pending, save the LLM's response as pluto.md.
	if (!fInitPendingPath.empty()) {
		std::string path = fInitPendingPath;
		fInitPendingPath.clear();

		// Only save if new messages were added (i.e. the LLM actually responded).
		auto* conv = _ActiveConv();
		if (conv && conv->Messages().size() > fInitMsgCount) {
			const auto& msgs = conv->Messages();
			// Find the last assistant message (only among new messages).
			for (int i = (int)msgs.size() - 1; i >= (int)fInitMsgCount; --i) {
				if (msgs[i].role == "assistant") {
					std::string content;
					for (const auto& b : msgs[i].content) {
						if (b.type == ContentBlock::TEXT)
							content += b.text;
					}
					if (!content.empty()) {
						// Strip markdown code fences if the LLM wrapped the output.
						if (content.find("```markdown") == 0 || content.find("```md") == 0) {
							size_t start = content.find('\n');
							if (start != std::string::npos) content = content.substr(start + 1);
						}
						if (content.size() > 3 && content.rfind("```") == content.size() - 3)
							content = content.substr(0, content.size() - 3);
						// Trim trailing whitespace.
						while (!content.empty() && (content.back() == '\n' || content.back() == ' '))
							content.pop_back();
						content += "\n";

						std::ofstream f(path, std::ios::trunc);
						if (f) {
							f << content;
							_Append("\nSaved " + path + "\n", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
							_Append("It will be loaded automatically on the next session.\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
							pluto::ApplyProjectMemory(*fConfig);

							// Show content in side panel.
							fSideTitle->SetText("pluto.md — Project Memory");
							int32 n = static_cast<int32>(content.size());
							fSideView->SetText(content.c_str(), n);
							BFont mono = *be_fixed_font;
							fSideView->SetFontAndColor(0, n, &mono, B_FONT_ALL, nullptr);
						} else {
							_Append("\nFailed to write " + path + "\n", rgb_color{245, 101, 101, 255});
						}
					}
					break;
				}
			}
		} else {
			_Append("\n/init failed — no response from model. "
				"Try switching to a different model and run /init --force\n\n", rgb_color{245, 101, 101, 255});
		}
	}

	// Notify if the window is in the background.
	auto* conv = _ActiveConv();
	bigtime_t elapsed = conv
		? system_time() - conv->GetTurnStartedAt() : 0;
	if (!IsActive() && elapsed > 3000000LL) {
		std::string text = preview;
		BNotification n(B_INFORMATION_NOTIFICATION);
		n.SetGroup("Pluto");
		std::string nTitle = "Pluto";
		if (conv && !conv->GetSessionTitle().empty())
			nTitle += " — " + conv->GetSessionTitle();
		else
			nTitle += " — turn complete";
		n.SetTitle(nTitle.c_str());
		if (text.empty()) text = "(no text reply)";
		if (text.size() > 200)
			text = text.substr(0, 200) + "\xe2\x80\xa6";
		n.SetContent(text.c_str());
		n.SetMessageID("pluto-turn");
		n.SetOnClickApp("application/x-vnd.Hay-pluto");
		n.Send(static_cast<bigtime_t>(8) * 1000000LL);
	}
	_RebuildSessionsMenu();
}

int MainWindow::RequestToolApproval(const std::string& tool_name,
                                     const std::string& input_preview) {
	if (fAlwaysApproved.count(tool_name)) return 1;
	BString text;
	text.SetToFormat("Tool: %s\n\n%s", tool_name.c_str(), input_preview.c_str());
	BAlert* alert = new BAlert("Pluto — tool approval",
		text.String(),
		"Deny", "Allow", "Always",
		B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_INFO_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	int32 rc = alert->Go();
	if (rc == 2) fAlwaysApproved.insert(tool_name);
	return static_cast<int>(rc);
}

void MainWindow::OnRagInject(const std::vector<std::string>& /*topics*/) {
	// Now handled by ConversationView.
}

void MainWindow::OnProviderStatus(const std::string& /*status*/) {
	// Now handled by ConversationView.
}

void MainWindow::OnTurnStarted() {
	_SetSendStopMode(true);
	fInput->MakeEditable(false);
	_UpdateTitle("thinking\xe2\x80\xa6");
	_UpdateStatus();
}

void MainWindow::OnTitleActivity(const std::string& activity) {
	_UpdateTitle(activity.c_str());
}

void MainWindow::_BuildMenuBar() {
	fMenuBar = new BMenuBar("menubar");

	BMenu* file = new BMenu("File");
	file->AddItem(new BMenuItem("New conversation",
		new BMessage(kMsgReset), 'N'));
	file->AddItem(new BMenuItem("New tab",
		new BMessage(kMsgNewTab), 'T'));
	file->AddItem(new BMenuItem("Close tab",
		new BMessage(kMsgCloseTab), 'W'));
	file->AddSeparatorItem();
	file->AddItem(new BMenuItem("Preferences…",
		new BMessage(kMsgPrefs), ','));
	file->AddItem(new BMenuItem("Schedules…",
		new BMessage(kMsgScheduleDialog)));
	file->AddItem(new BMenuItem("Routines…",
		new BMessage(kMsgRoutineDialog)));
	{
		BMenu* runRoutine = new BMenu("Run routine");
		auto routines = pluto::ListRoutines();
		if (routines.empty()) {
			BMenuItem* empty = new BMenuItem("(no routines)", nullptr);
			empty->SetEnabled(false);
			runRoutine->AddItem(empty);
		} else {
			for (const auto& r : routines) {
				BMessage* msg = new BMessage(kMsgRunRoutine);
				msg->AddString("name", r.name.c_str());
				std::string label = r.name;
				if (!r.enabled) label += "  (paused)";
				runRoutine->AddItem(new BMenuItem(label.c_str(), msg));
			}
		}
		file->AddItem(runRoutine);
	}
	file->AddItem(new BMenuItem("Agents…",
		new BMessage(kMsgAgentDialog)));
	file->AddSeparatorItem();
	{
		pluto::OAuthTokens tok = pluto::LoadOAuthTokens();
		if (tok.empty()) {
			file->AddItem(new BMenuItem("Login with Claude account…",
				new BMessage(kMsgOAuthLogin)));
		} else {
			file->AddItem(new BMenuItem("Logout from Claude account",
				new BMessage(kMsgOAuthLogout)));
		}
	}
	file->AddSeparatorItem();
	BMenu* exportMenu = new BMenu("Export as…");
	exportMenu->AddItem(new BMenuItem("Markdown",
		new BMessage(kMsgExportMd)));
	exportMenu->AddItem(new BMenuItem("JSONL",
		new BMessage(kMsgExportJsonl)));
	file->AddItem(exportMenu);
	file->AddSeparatorItem();
	file->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(file);

	BMenu* view = new BMenu("View");
	view->AddItem(new BMenuItem("Session browser…",
		new BMessage(kMsgSessionBrowser), 'B'));
	view->AddItem(new BMenuItem("Find…",
		new BMessage(kMsgFind), 'F'));
	view->AddSeparatorItem();
	view->AddItem(new BMenuItem("Zoom in",
		new BMessage(kMsgZoomIn), '='));
	view->AddItem(new BMenuItem("Zoom out",
		new BMessage(kMsgZoomOut), '-'));
	view->AddItem(new BMenuItem("Reset zoom",
		new BMessage(kMsgZoomReset), '0'));
	view->AddSeparatorItem();
	{
		BMenuItem* nextTab = new BMenuItem("Next tab",
			new BMessage(kMsgNextTab));
		nextTab->SetShortcut(B_RIGHT_ARROW, B_COMMAND_KEY);
		view->AddItem(nextTab);
		BMenuItem* prevTab = new BMenuItem("Previous tab",
			new BMessage(kMsgPrevTab));
		prevTab->SetShortcut(B_LEFT_ARROW, B_COMMAND_KEY);
		view->AddItem(prevTab);
	}
	fMenuBar->AddItem(view);

	BMenu* session = new BMenu("Session");
	session->AddItem(new BMenuItem("Save session",
		new BMessage(kMsgSaveSession), 'S'));
	session->AddItem(new BMenuItem("Resume last",
		new BMessage(kMsgResumeLast), 'R'));
	fSessionsMenu = new BMenu("Recent sessions");
	session->AddItem(fSessionsMenu);
	session->AddSeparatorItem();
	session->AddItem(new BMenuItem("Fork to new tab",
		new BMessage('plFk')));
	session->AddItem(new BMenuItem("Reset history",
		new BMessage(kMsgReset)));
	fMenuBar->AddItem(session);
	_RebuildSessionsMenu();

	fMcpMenu = new BMenu("MCP");
	fMenuBar->AddItem(fMcpMenu);
	_RebuildMcpMenu();
	fMcpMenu->AddSeparatorItem();
	fMcpMenu->AddItem(new BMenuItem("Discover servers…",
		new BMessage(kMsgMcpDiscover)));
	fMcpMenu->AddSeparatorItem();
	fMcpMenu->AddItem(new BMenuItem("Peer instances…",
		new BMessage(kMsgPeerMenu)));

	BMenu* help = new BMenu("Help");
	help->AddItem(new BMenuItem("User guide",
		new BMessage(kMsgUserGuide)));
	help->AddSeparatorItem();
	help->AddItem(new BMenuItem("Install Deskbar icon",
		new BMessage(kMsgDeskbarInstall)));
	help->AddItem(new BMenuItem("Remove Deskbar icon",
		new BMessage(kMsgDeskbarRemove)));
	help->AddSeparatorItem();
	help->AddItem(new BMenuItem("Install Tracker add-on",
		new BMessage(kMsgTrackerAddonInstall)));
	help->AddItem(new BMenuItem("Remove Tracker add-on",
		new BMessage(kMsgTrackerAddonRemove)));
	help->AddSeparatorItem();
	help->AddItem(new BMenuItem("Usage statistics" B_UTF8_ELLIPSIS,
		new BMessage('plUs')));
	help->AddSeparatorItem();
	help->AddItem(new BMenuItem("About Pluto…",
		new BMessage(kMsgAbout)));
	fMenuBar->AddItem(help);
}

void MainWindow::_RebuildMcpMenu() {
	if (!fMcpMenu) return;
	while (BMenuItem* it = fMcpMenu->RemoveItem((int32)0)) delete it;

	if (fMcpClients.empty()) {
		BMenuItem* none = new BMenuItem("(no servers connected)", nullptr);
		none->SetEnabled(false);
		fMcpMenu->AddItem(none);
		return;
	}
	for (size_t i = 0; i < fMcpClients.size(); ++i) {
		BString label;
		label.SetToFormat("%s   (%ld tools)",
			fMcpClients[i]->server_name().c_str(),
			(long)fMcpClients[i]->tools().size());
		auto* msg = new BMessage(kMsgMcpInfo);
		msg->AddInt32("index", (int32)i);
		fMcpMenu->AddItem(new BMenuItem(label.String(), msg));
	}
}

void MainWindow::_ShowMcpServerInfo(int32 index) {
	if (index < 0 || (size_t)index >= fMcpClients.size()) return;
	auto* c = fMcpClients[index];

	// Build a markdown document and render in the side panel.
	std::string md;
	md += "# ";
	md += c->server_name();
	md += "\n\n";
	md += "**";
	md += std::to_string(c->tools().size());
	md += " tools**\n\n";

	for (const auto& t : c->tools()) {
		md += "- **";
		md += t.name;
		md += "**";
		if (!t.description.empty()) {
			// First line of description
			size_t nl = t.description.find('\n');
			std::string d = nl == std::string::npos
				? t.description : t.description.substr(0, nl);
			md += " — ";
			md += d;
		}
		md += "\n";
	}

	BString title;
	title.SetToFormat("MCP: %s  (%ld tools)",
		c->server_name().c_str(), (long)c->tools().size());
	fSideTitle->SetText(title.String());
	_RenderMarkdown(md);
}

// Rendering methods (_Append, _EmitUserHeader, _EmitAssistantHeader,
// _FinalizeAssistantBlock, _RenderAssistantInline, _FlushMdCodeBlockImpl,
// _RenderAssistantLine, _FeedStreamingMarkdown, _FlushStreamingMarkdown,
// _ResetStreamingMarkdown, _RenderAssistantMarkdown, _ShowCursor,
// _HideCursor) have been moved to ConversationView.

// ----- side panel -----

void MainWindow::_ResetSidePanel() {
	fSideTitle->SetText("\xF0\x9F\x93\x8B Detail");  // 📋
	const char* txt =
		"Tool outputs appear here in full.\n\n"
		"After every tool call the untruncated\n"
		"result shows on this side so you can\n"
		"inspect files, diffs and long outputs\n"
		"without scrolling the conversation.\n\n"
		"\xE2\x80\xA2 Click \xE2\x96\xB8 to expand collapsed blocks\n"
		"\xE2\x80\xA2 Click links to open in browser\n"
		"\xE2\x80\xA2 Drop files anywhere on the window";
	fSideView->SetText(txt);
	int32 n = fSideView->TextLength();
	BFont f = *be_plain_font;
	f.SetSize(f.Size() + fFontDelta);
	rgb_color c = ui_color(B_DOCUMENT_TEXT_COLOR);
	fSideView->SetFontAndColor(0, n, &f, B_FONT_ALL, &c);
}

void MainWindow::_UpdateSidePanel(const std::string& title,
                                  const std::string& body,
                                  bool is_error) {
	BString t;
	long kb = (long)body.size() / 1024;
	if (kb > 0)
		t.SetToFormat("\xF0\x9F\x94\xA7 %s  (%ld KB)",
			title.empty() ? "tool" : title.c_str(), kb);
	else
		t.SetToFormat("\xF0\x9F\x94\xA7 %s  (%ld B)",
			title.empty() ? "tool" : title.c_str(), (long)body.size());
	fSideTitle->SetText(t.String());

	// Try to detect and show image from tool result.
	// Scan the body for any file path ending with an image extension.
	bool showed_image = false;
	if (!is_error && !body.empty()) {
		std::string path;
		// First try: entire body is a path (trimmed).
		{
			std::string trimmed = body;
			while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\n'))
				trimmed.erase(0, 1);
			while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n'))
				trimmed.pop_back();
			for (const char* ext : {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp"}) {
				if (trimmed.size() > strlen(ext)
				    && trimmed.compare(trimmed.size() - strlen(ext), strlen(ext), ext) == 0) {
					path = trimmed;
					break;
				}
			}
		}
		// Second try: scan for /path/to/file.ext pattern in the text.
		if (path.empty()) {
			for (const char* ext : {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp"}) {
				size_t pos = body.find(ext);
				while (pos != std::string::npos) {
					// Walk backwards to find the start of the path.
					size_t end = pos + strlen(ext);
					size_t start = pos;
					while (start > 0 && body[start - 1] != ' ' && body[start - 1] != '\n'
					       && body[start - 1] != '\t' && body[start - 1] != '"'
					       && body[start - 1] != '\'')
						--start;
					std::string candidate = body.substr(start, end - start);
					if (candidate.size() > 4 && candidate[0] == '/') {
						struct stat st;
						if (::stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
							path = candidate;
							break;
						}
					}
					pos = body.find(ext, end);
				}
				if (!path.empty()) break;
			}
		}

		if (!path.empty()) {
			BFile file(path.c_str(), B_READ_ONLY);
			if (file.InitCheck() == B_OK) {
				off_t sz = 0;
				file.GetSize(&sz);
				if (sz > 0 && sz < 20 * 1024 * 1024) {
					BMallocIO memIO;
					memIO.SetSize(sz);
					ssize_t bytesRead = file.Read(
						const_cast<void*>(memIO.Buffer()), sz);
					if (bytesRead < sz)
						memIO.SetSize(bytesRead > 0 ? bytesRead : 0);
					memIO.Seek(0, SEEK_SET);

					BBitmap* bitmap = nullptr;
					BTranslatorRoster* roster = BTranslatorRoster::Default();
					BBitmapStream stream;
					if (roster->Translate(&memIO, nullptr, nullptr,
					    &stream, B_TRANSLATOR_BITMAP) == B_OK) {
						stream.DetachBitmap(&bitmap);
					}
					if (bitmap) {
						fImagePreview->SetBitmap(bitmap);
						if (fImagePreview->IsHidden())
							fImagePreview->Show();
						showed_image = true;
					}
				}
			}
		}
	}

	if (!showed_image && fImagePreview->HasBitmap()) {
		fImagePreview->SetBitmap(nullptr);
		if (!fImagePreview->IsHidden()) fImagePreview->Hide();
	}

	if (is_error) {
		std::string err_display = "\xE2\x9A\xA0 Error\n\n";  // ⚠
		err_display += body;
		fSideView->SetText(err_display.data(), (int32)err_display.size());
		// Style the header.
		int32 header_end = 9;  // "⚠ Error\n\n"
		BFont bold_f = *be_plain_font;
		bold_f.SetFace(B_BOLD_FACE);
		bold_f.SetSize(bold_f.Size() + fFontDelta + 2);
		fSideView->SetFontAndColor(0, header_end, &bold_f, B_FONT_ALL, &rgb_color{245, 101, 101, 255});
		// Style the body.
		int32 n = fSideView->TextLength();
		BFont mono_f = *be_fixed_font;
		mono_f.SetSize(mono_f.Size() + fFontDelta);
		fSideView->SetFontAndColor(header_end, n, &mono_f, B_FONT_ALL, &rgb_color{245, 101, 101, 255});
		fSideView->ScrollToOffset(0);
	} else {
		// Render as markdown — tool output often contains structured text.
		_RenderMarkdown(body);
	}
}

// ----- status -----

void MainWindow::_UpdateStatus() {
	auto* conv = _ActiveConv();
	if (!conv || !fStatus) return;
	const Config& tc = conv->GetConfig();
	long tIn = conv->GetTokensIn();
	long tOut = conv->GetTokensOut();
	long ctx_used = tIn + tOut;
	long ctx_max  = tc.context_window > 0
		? tc.context_window : 200000;
	int pct = ctx_max > 0
		? static_cast<int>(100.0f * ctx_used / ctx_max) : 0;
	if (pct > 100) pct = 100;

	BString cache_part;
	long cRead = conv->GetLastCacheRead();
	long cCreate = conv->GetLastCacheCreate();
	if (cRead > 0 || cCreate > 0) {
		cache_part.SetToFormat("   ·   cache R%ld/C%ld",
			cRead, cCreate);
	}

	double cost = conv->EstimateCost();
	BString cost_part;
	if (cost > 0.0001) cost_part.SetToFormat("   ·   $%.4f", cost);

	// Workspace trust indicator
	BString ws_part;
	{
		std::string ws = pluto::WorkspaceRoot();
		if (!ws.empty()) {
			bool trusted = pluto::IsWorkspaceTrusted(ws)
			            || pluto::IsSessionTrusted();
			size_t sl = ws.rfind('/');
			std::string leaf = sl != std::string::npos ? ws.substr(sl + 1) : ws;
			ws_part.SetToFormat("%s %s   ·   ",
				trusted ? "\xE2\x9C\x93" : "\xE2\x9A\xA0",
				leaf.c_str());
		}
	}

	bool busy = conv->IsBusy();
	std::string curTool = conv->GetCurrentTool();

	// Elapsed time string for busy state.
	BString elapsed_part;
	if (busy) {
		bigtime_t elapsed = system_time() - conv->GetTurnStartedAt();
		int secs = static_cast<int>(elapsed / 1000000LL);
		if (secs < 60)
			elapsed_part.SetToFormat("   ·   %ds", secs);
		else
			elapsed_part.SetToFormat("   ·   %dm%02ds", secs / 60, secs % 60);
	}

	BString s;
	const char* state = busy
		? (curTool.empty() ? "Thinking…" : "")
		: "Ready";
	if (busy && !curTool.empty()) {
		s.SetToFormat(
			"%sRunning tool: %s%s   ·   %s   ·   in %ld / out %ld tok   ·   ctx %d%%%s%s",
			ws_part.String(),
			curTool.c_str(), elapsed_part.String(),
			tc.model.c_str(),
			tIn, tOut, pct,
			cache_part.String(), cost_part.String());
	} else {
		s.SetToFormat(
			"%s%s%s   ·   %s   ·   in %ld / out %ld tok   ·   ctx %d%%%s%s",
			ws_part.String(),
			state, elapsed_part.String(),
			tc.model.c_str(),
			tIn, tOut, pct,
			cache_part.String(), cost_part.String());
	}
	fStatus->SetText(s.String());

	if (fGauge) fGauge->SetValues(ctx_used, ctx_max);

	// Context window usage warnings — notify once per threshold.
	if (pct >= 90 && fCtxWarningShown < 90) {
		fCtxWarningShown = 90;
		_Append("\xe2\x9a\xa0 Context window 90% full — "
			"consider starting a new conversation (/reset)\n\n",
			rgb_color{245, 101, 101, 255}, B_BOLD_FACE);
		BNotification n(B_IMPORTANT_NOTIFICATION);
		n.SetGroup("Pluto");
		n.SetTitle("Context almost full");
		n.SetContent("Context window is 90% full. Start a new "
			"conversation soon to avoid truncation.");
		n.SetMessageID("pluto-ctx-warn");
		n.Send(10000000LL);
	} else if (pct >= 75 && fCtxWarningShown < 75) {
		fCtxWarningShown = 75;
		_Append("\xe2\x9a\xa0 Context window 75% full\n\n",
			(rgb_color){230, 140, 40, 255});
	} else if (pct >= 50 && fCtxWarningShown < 50) {
		fCtxWarningShown = 50;
		_Append("\xe2\x9a\xa0 Context window 50% full\n\n",
			(rgb_color){215, 180, 40, 255});
	}
}

// _EstimateCost has been moved to ConversationView.

// ----- message routing -----

void MainWindow::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgSend:
			_SendCurrentInput();
			break;

		case kMsgReset:
			_ResetConversation();
			break;

		case kMsgNewTab:
			_CreateTab();
			break;

		case 'plFk': {
			// Fork: copy current conversation to a new tab.
			auto* cur = _ActiveConv();
			if (!cur || cur->Messages().empty()) break;
			auto* fork = _CreateTab();
			if (fork)
				fork->LoadMessages(cur->Messages());
			break;
		}

		case kMsgOpenNewTab: {
			// A second Pluto instance forwarded its launch to us.
			auto* conv = _CreateTab();
			Activate();
			// If the message carries a prompt, inject it.
			const char* prompt = nullptr;
			if (msg->FindString("prompt", &prompt) == B_OK
			    && prompt && *prompt && conv) {
				ContentBlock tb;
				tb.type = ContentBlock::TEXT;
				tb.text = prompt;
				conv->SendInput({tb});
			}
			// Reply OK so the sender knows forwarding succeeded.
			BMessage reply(B_REPLY);
			reply.AddInt32("error", B_OK);
			msg->SendReply(&reply);
			break;
		}

		case kMsgCloseTab:
			_CloseTab();
			break;

		case 'wiki': {
			const char* result = nullptr;
			if (msg->FindString("result", &result) == B_OK && result) {
				// Strip ANSI escape codes for clean display.
				std::string clean;
				const char* p = result;
				while (*p) {
					if (*p == '\x1b' && *(p+1) == '[') {
						p += 2;
						while (*p && !std::isalpha((unsigned char)*p)) p++;
						if (*p) p++;
					} else {
						clean += *p++;
					}
				}
				_Append(clean + "\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			}
			break;
		}

		case kMsgTabChanged:
			_OnTabChanged();
			break;

		case kMsgNextTab:
			if (fTabView && fTabView->CountTabs() > 1) {
				int32 next = (fTabView->Selection() + 1) % fTabView->CountTabs();
				fTabView->Select(next);
			}
			break;

		case kMsgPrevTab:
			if (fTabView && fTabView->CountTabs() > 1) {
				int32 n = fTabView->CountTabs();
				int32 prev = (fTabView->Selection() + n - 1) % n;
				fTabView->Select(prev);
			}
			break;

		case kMsgTabCtxClose: {
			int32 idx = 0;
			if (msg->FindInt32("tab_index", &idx) == B_OK && fTabView) {
				fTabView->Select(idx);
				_CloseTab();
			}
			break;
		}
		case kMsgTabCtxCloseOthers: {
			int32 keep = 0;
			if (msg->FindInt32("tab_index", &keep) == B_OK)
				_CloseOtherTabs(keep);
			break;
		}
		case kMsgTabCtxRename: {
			int32 idx = 0;
			if (msg->FindInt32("tab_index", &idx) == B_OK)
				_RenameTab(idx);
			break;
		}

		case kMsgAbout:
			_ShowAbout();
			break;

		case 'plUs': {
			auto* dlg = new gui::TelemetryDialog();
			dlg->Show();
			break;
		}

		case kMsgUserGuide: {
			// Find the user guide relative to the binary location, or
			// fall back to a well-known path.
			const char* paths[] = {
				"docs/user-guide.md",
				"../docs/user-guide.md",
				"/boot/home/Desktop/pluto/docs/user-guide.md",
				nullptr
			};
			std::string guide_path;
			for (const char** p = paths; *p; ++p) {
				std::ifstream f(*p);
				if (f) { guide_path = *p; break; }
			}
			if (guide_path.empty()) {
				BAlert* a = new BAlert("Pluto",
					"User guide not found.\n\n"
					"Expected at docs/user-guide.md",
					"OK");
				a->Go(nullptr);
				break;
			}
			// Load into side panel with markdown rendering.
			std::ifstream gf(guide_path);
			std::string content((std::istreambuf_iterator<char>(gf)),
			                     std::istreambuf_iterator<char>());
			fSideTitle->SetText("User guide");
			_RenderMarkdown(content);
			break;
		}

		case kMsgMcpInfo: {
			int32 idx = -1;
			msg->FindInt32("index", &idx);
			_ShowMcpServerInfo(idx);
			break;
		}

		case kMsgMcpDiscover: {
			auto found = pluto::DiscoverMcpServers();
			if (found.empty()) {
				BAlert* a = new BAlert("MCP discover",
					"No new MCP servers found on localhost.\n\n"
					"Scanned ports: 2607 (pippo), 3000, 4000, 8000, 8080.",
					"OK");
				a->Go(nullptr);
				break;
			}
			BString body;
			body.SetToFormat("Found %ld server(s):\n\n",
				(long)found.size());
			for (const auto& s : found) {
				body += "  ";
				body += s.name.c_str();
				body += "  —  ";
				body += s.url.c_str();
				body += " (";
				body += s.type.c_str();
				body += ")\n";
			}
			body += "\nTo connect, add them via:\n"
			        "  pluto mcp add <name> --url <url> --type sse --scope user\n"
			        "\nthen restart Pluto-gui.";
			BAlert* a = new BAlert("MCP discover", body.String(), "OK");
			a->Go(nullptr);
			break;
		}

		case kMsgSaveSession:
			_SaveSession();
			break;

		case kMsgResumeLast: {
			SessionMetadata m = SessionStore::most_recent();
			if (!m.path.empty()) _LoadSessionFromPath(m.path);
			break;
		}

		case kMsgLoadSession: {
			const char* path = "";
			if (msg->FindString("path", &path) == B_OK && *path)
				_LoadSessionFromPath(path);
			break;
		}

		case kMsgPrefs:
			_OpenSettings();
			break;

		case 'grPr': {
			// Graph scan progress — update status bar.
			const char* status = nullptr;
			msg->FindString("status", &status);
			if (status && fStatus)
				fStatus->SetText(status);
			break;
		}

		case 'grVw': {
			// Open graph viewer window.
			const char* path = nullptr;
			msg->FindString("path", &path);
			if (path) {
				auto graph = pluto::map::Graph::LoadFromFile(path);
				if (!graph.Nodes().empty()) {
					auto* win = new pluto::gui::GraphWindow(graph);
					win->Show();
					std::string note = "  \xe2\x9c\x93 Graph: "
						+ std::to_string(graph.Nodes().size()) + " nodes, "
						+ std::to_string(graph.Edges().size()) + " edges\n\n";
					_Append(note, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
					_UpdateStatus();
				}
			}
			break;
		}

		case kMsgExportMd:
			_ExportMarkdown();
			break;
		case kMsgExportJsonl:
			_ExportJsonl();
			break;

		case kMsgSettingsAccepted: {
			const char* provider = "";
			const char* base_url = "";
			const char* api_key_env = "";
			const char* model = "";
			const char* sys = "";
			int32 mt = 0, cw = 0;
			bool cache = true;
			msg->FindString("provider", &provider);
			msg->FindString("base_url", &base_url);
			msg->FindString("api_key_env", &api_key_env);
			msg->FindString("model", &model);
			msg->FindString("system", &sys);
			msg->FindInt32("max_tokens", &mt);
			msg->FindInt32("context_window", &cw);
			const char* color_scheme = "auto";
			msg->FindBool("cache", &cache);
			msg->FindString("color_scheme", &color_scheme);
			// Update the global config (persisted + used for new tabs).
			if (provider && *provider) fConfig->provider = provider;
			if (base_url) fConfig->base_url = base_url;
			if (api_key_env) fConfig->api_key_env = api_key_env;
			if (model && *model) fConfig->model = model;
			fConfig->user_system_prompt = sys ? sys : "";
			fConfig->system_prompt = fConfig->user_system_prompt;
			pluto::ApplyProjectMemory(*fConfig);
			pluto::ApplySkills(*fConfig);
			if (pluto::RagEntryCount() > 0) pluto::ApplyRagHint(*fConfig);
			if (mt > 0) fConfig->max_tokens = mt;
			if (cw > 0) fConfig->context_window = cw;
			fConfig->cache = cache;
			fConfig->color_scheme = color_scheme ? color_scheme : "auto";
			pluto::SaveConfig(*fConfig);
			_InitPalette();

			// Apply to the active tab: update its config + recreate provider.
			if (auto* c = _ActiveConv()) {
				Config& tc = c->MutableConfig();
				if (provider && *provider) tc.provider = provider;
				if (base_url) tc.base_url = base_url;
				if (api_key_env) tc.api_key_env = api_key_env;
				if (model && *model) tc.model = model;
				tc.user_system_prompt = sys ? sys : "";
				tc.system_prompt = tc.user_system_prompt;
				pluto::ApplyProjectMemory(tc);
				pluto::ApplySkills(tc);
				if (pluto::RagEntryCount() > 0) pluto::ApplyRagHint(tc);
				// Copy hooks from freshly-loaded global config.
				tc.hooks_pre_tool = fConfig->hooks_pre_tool;
				tc.hooks_post_tool = fConfig->hooks_post_tool;
				tc.hooks_user_prompt = fConfig->hooks_user_prompt;
				if (mt > 0) tc.max_tokens = mt;
				if (cw > 0) tc.context_window = cw;
				tc.cache = cache;

				auto p = MakeProviderForConfig(tc);
				if (p) {
					c->SetProvider(std::move(p));
					std::string note = "\n  \xe2\x9c\x93 Switched to "
						+ friendly_provider(tc);
					if (!tc.model.empty())
						note += " / " + tc.model;
					note += "\n\n";
					_Append(note, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
				}
			}
			_UpdateTitle();
			_UpdateStatus();
			OnTitleChanged("");  // refresh tab labels with new provider:model
			break;
		}

		case kMsgStop:
			if (_ActiveConv()) _ActiveConv()->Cancel();
			break;

		case kMsgZoomIn:
			fFontDelta += 1;
			fConfig->font_delta = fFontDelta;
			pluto::SaveConfig(*fConfig);
			_ApplyFontZoom();
			break;
		case kMsgZoomOut:
			fFontDelta -= 1;
			fConfig->font_delta = fFontDelta;
			pluto::SaveConfig(*fConfig);
			_ApplyFontZoom();
			break;
		case kMsgZoomReset:
			fFontDelta = 0;
			fConfig->font_delta = 0;
			pluto::SaveConfig(*fConfig);
			_ApplyFontZoom();
			break;

		case kMsgFind:     _ShowFindBar(); break;
		case kMsgSessionBrowser: {
			BRect frame(Frame());
			frame.InsetBy(60, 40);
			auto* browser = new SessionBrowser(frame, this);
			browser->CenterIn(Frame());
			browser->Show();
			break;
		}
		case kMsgFindNext: _FindInConversation(); break;
		case kMsgFindHide: _HideFindBar(); break;

		case B_REFS_RECEIVED: {
			std::fprintf(stderr, "[pluto] B_REFS_RECEIVED received\n");
			{
				const char* dbg_cwd = nullptr;
				if (msg->FindString("ask_pluto_cwd", &dbg_cwd) == B_OK)
					std::fprintf(stderr, "[pluto]   ask_pluto_cwd = %s\n",
						dbg_cwd ? dbg_cwd : "(null)");
				else
					std::fprintf(stderr, "[pluto]   no ask_pluto_cwd field\n");
			}
			entry_ref ref;
			int32 i = 0;
			bool offered_trust = false;
			while (msg->FindRef("refs", i++, &ref) == B_OK) {
				BPath p(&ref);
				if (p.InitCheck() != B_OK || !p.Path()) continue;

				// Workspace integration: if file is outside workspace,
				// offer to trust its parent directory.
				if (!offered_trust
				    && !pluto::WorkspaceAllows(p.Path())) {
					BPath parent;
					if (p.GetParent(&parent) == B_OK) {
						BString q;
						q.SetToFormat(
							"The file \"%s\" is outside the workspace.\n\n"
							"Trust directory \"%s\"?",
							p.Leaf(), parent.Path());
						BAlert* alert = new BAlert("Workspace",
							q.String(),
							"No", "This session", "Always",
							B_WIDTH_AS_USUAL, B_OFFSET_SPACING,
							B_WARNING_ALERT);
						alert->SetShortcut(0, B_ESCAPE);
						int32 rc = alert->Go();
						if (rc == 1) pluto::TrustSession();
						else if (rc == 2) pluto::TrustWorkspace(parent.Path());
						offered_trust = true;
						_UpdateStatus();
					}
				}

				std::string mime = pluto::image_media_type(p.Path());
				std::string afmt = pluto::audio_format(p.Path());
				if (!mime.empty())
					_AttachImageToInput(p.Path(), mime);
				else if (!afmt.empty())
					_AttachMediaToInput(p.Path(), afmt, "audio");
				else if (pluto::is_pdf(p.Path()))
					_AttachMediaToInput(p.Path(), "application/pdf", "pdf");
				else
					_AttachFileToInput(p.Path());
			}

			// Bitmap clip: no refs but has bitmap data (e.g. drag from
			// ShowImage, WonderBrush, Screenshot, clipboard paste).
			// Translate to PNG in a temp file and attach as image.
			if (i == 1) {  // i starts at 0, incremented before first check; 1 = no refs found
				const void* data = nullptr;
				ssize_t size = 0;
				// Try archived BBitmap in the message.
				if (msg->FindData("image/bitmap", B_MESSAGE_TYPE, &data, &size) == B_OK
				    || msg->FindData("image/x-be-bitmap", B_RAW_TYPE, &data, &size) == B_OK
				    || msg->FindData("image/x-vnd.Be-bitmap", B_RAW_TYPE, &data, &size) == B_OK) {
					_HandleBitmapClipDrop(data, size);
				} else {
					// Try BBitmap archived as a BMessage inside the drag.
					BMessage archive;
					if (msg->FindMessage("image/bitmap", &archive) == B_OK
					    || msg->FindMessage("image/x-be-bitmap", &archive) == B_OK) {
						BBitmap* bmp = new BBitmap(&archive);
						if (bmp && bmp->IsValid()) {
							_SaveBitmapAndAttach(bmp);
						}
						delete bmp;
					}
				}
			}

			fInput->MakeFocus(true);

			// Tracker add-on: switch workspace to the folder the
			// user right-clicked in, so all tools resolve relative
			// to that directory.
			{
				const char* cwd = nullptr;
				if (msg->FindString("ask_pluto_cwd", &cwd) == B_OK
				    && cwd && *cwd) {
					std::string prev = pluto::WorkspaceRoot();
					std::string next(cwd);
					std::fprintf(stderr, "[pluto]   workspace prev=%s next=%s\n",
						prev.c_str(), next.c_str());
					if (next != prev) {
						pluto::WorkspaceSetRoot(next);
						std::fprintf(stderr, "[pluto]   workspace changed to %s\n",
							pluto::WorkspaceRoot().c_str());

						// Offer trust for the new workspace if needed.
						if (!pluto::IsWorkspaceTrusted(next)
						    && !pluto::IsSessionTrusted()) {
							BString q;
							q.SetToFormat(
								"Ask Pluto wants to work in:\n\n"
								"  %s\n\n"
								"Trust this directory?", cwd);
							BAlert* alert = new BAlert("Workspace",
								q.String(),
								"No", "This session", "Always",
								B_WIDTH_AS_USUAL, B_OFFSET_SPACING,
								B_WARNING_ALERT);
							alert->SetShortcut(0, B_ESCAPE);
							int32 rc = alert->Go();
							if (rc == 1) pluto::TrustSession();
							else if (rc == 2) pluto::TrustWorkspace(next);
						}

						_UpdateTitle();
						_UpdateStatus();

						// Reload project-level config from new workspace.
						*fConfig = pluto::LoadConfig();
						pluto::ApplyProjectMemory(*fConfig);
						pluto::ApplySkills(*fConfig);
					}
				}
			}

			// If sent from the Tracker add-on, inject the prompt and send.
			{
				const char* ask_prompt = nullptr;
				if (msg->FindString("ask_pluto_prompt", &ask_prompt) == B_OK
				    && ask_prompt && *ask_prompt && !_ActiveConv()->IsBusy()) {
					fInput->SetText(ask_prompt);
					_SendCurrentInput();
				}
			}
			break;
		}

		case kMsgImageClear:
			_ClearPendingImages();
			break;

		case 'plIS': {
			// Save image from the preview panel.
			if (fImagePreview && fImagePreview->HasBitmap()) {
				BFilePanel* panel = new BFilePanel(B_SAVE_PANEL,
					new BMessenger(this), nullptr, 0, false);
				panel->SetSaveText("pluto_image.png");
				panel->Show();
			}
			break;
		}

		case B_SAVE_REQUESTED: {
			entry_ref dir;
			const char* name = nullptr;
			if (msg->FindRef("directory", &dir) == B_OK
			    && msg->FindString("name", &name) == B_OK
			    && fImagePreview && fImagePreview->HasBitmap()) {
				_SaveImageToFile(&dir, name);
			}
			break;
		}

		case kMsgPlayerPlay: {
			// Launch the audio/video file with the system's default player.
			if (!fPlayerPath.empty()) {
				entry_ref ref;
				if (get_ref_for_path(fPlayerPath.c_str(), &ref) == B_OK) {
					be_roster->Launch(&ref);
				}
			}
			break;
		}

		case kMsgPlayerStop: {
			// We can't stop an external player, but we can hide our bar.
			fPlayerBar->Hide();
			fPlayerPath.clear();
			break;
		}

		case kMsgImageRemove: {
			int32 idx = -1;
			if (msg->FindInt32("index", &idx) == B_OK
			    && idx >= 0 && (size_t)idx < fPendingImages.size()) {
				fPendingImages.erase(fPendingImages.begin() + idx);
				_UpdateImageChips();
			}
			break;
		}


		case kMsgToolToggle: {
			int32 offset = -1;
			float x = 100.0f;
			msg->FindInt32("offset", &offset);
			msg->FindFloat("x", &x);
			// Only toggle when clicking in the arrow column, not when
			// selecting text inside the header.
			if (x > 28) break;
			{
				// TODO(R2b): delegate tool block toggle to fConv
				// int hit = _ActiveConv()->ToolBlocks().hit_test(offset);
				// if (hit >= 0) _ActiveConv()->ToggleToolBlock(hit);
			}
			break;
		}

		case kMsgSideLink: {
			int32 offset = -1;
			msg->FindInt32("offset", &offset);
			for (const auto& lr : fSideLinks) {
				if (offset >= lr.start && offset < lr.end) {
					if (!lr.url.empty() && lr.url[0] == '#') {
						std::string anchor = lr.url.substr(1);
						auto it = fSideAnchors.find(anchor);
						if (it != fSideAnchors.end()) {
							// PointAt gives the Y coordinate of the
							// text at offset; ScrollTo puts that Y at
							// the top of the viewport.
							BPoint pt = fSideView->PointAt(it->second);
							fSideView->ScrollTo(0, pt.y);
							fSideView->Select(it->second, it->second);
						}
					} else if (!lr.url.empty()) {
						std::string cmd = "open \"" + lr.url + "\" &";
						::system(cmd.c_str());
					}
					break;
				}
			}
			break;
		}

		case kMsgDeskbarInstall:
			if (InstallDeskbarReplicant())
				_Append("Deskbar icon installed.\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			else
				_Append("Could not install Deskbar icon.\n", rgb_color{245, 101, 101, 255});
			break;

		case kMsgDeskbarRemove:
			if (RemoveDeskbarReplicant())
				_Append("Deskbar icon removed.\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			else
				_Append("Could not remove Deskbar icon.\n", rgb_color{245, 101, 101, 255});
			break;

		case kMsgTrackerAddonInstall: {
		std::string src = "Ask Pluto";
		// Try to find the add-on binary near the Pluto-gui binary.
		image_info info;
		int32 cookie = 0;
		while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
			if (info.type == B_APP_IMAGE) {
				BPath dir(info.name);
				dir.GetParent(&dir);
				BPath addon(dir.Path(), "Ask Pluto");
				struct stat st;
				if (stat(addon.Path(), &st) == 0) {
					src = addon.Path();
					break;
				}
			}
		}

		std::string dst_dir = HomeDir() + "/config/non-packaged/add-ons/Tracker";
		// Use native Haiku APIs instead of system() for security
		BDirectory dir(dst_dir.c_str());
		if (dir.InitCheck() != B_OK) {
			// Create directory hierarchy
			BPath path(dst_dir.c_str());
			path.GetParent(&path);
			status_t err = path.CreateDirectory(dst_dir.c_str(), 0755);
			if (err != B_OK) {
				_Append("Could not create Tracker add-ons directory.
", rgb_color{245, 101, 101, 255});
				break;
			}
		}

		// Copy the add-on file
		BEntry srcEntry(src.c_str());
		if (srcEntry.InitCheck() != B_OK) {
			_Append("Could not find add-on binary.
", rgb_color{245, 101, 101, 255});
			break;
		}

		BDirectory dstDir(dst_dir.c_str());
		if (dstDir.InitCheck() == B_OK) {
			BEntry dstEntry;
			if (srcEntry.Copy(&dstDir, "Ask Pluto", &dstEntry) == B_OK) {
				_Append("Tracker add-on "Ask Pluto" installed.
", ui_color(B_DOCUMENT_TEXT_COLOR));
				_Append("Right-click files in Tracker to use it.
", ui_color(B_DOCUMENT_TEXT_COLOR));
			} else {
				_Append("Could not copy add-on to Tracker directory.
", rgb_color{245, 101, 101, 255});
				_Append("Build it first: make 'Ask Pluto'
", rgb_color{245, 101, 101, 255});
			}
		} else {
			_Append("Could not access Tracker add-ons directory.
", rgb_color{245, 101, 101, 255});
		}
		break;
	}
					}
				}
			}
		}

		case kMsgTrackerAddonRemove: {
			std::string path = HomeDir()
				+ "/config/non-packaged/add-ons/Tracker/Ask Pluto";
			if (std::remove(path.c_str()) == 0)
				_Append("Tracker add-on removed.\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			else
				_Append("Tracker add-on not found.\n", rgb_color{245, 101, 101, 255});
			break;
		}

		case kMsgSessionPicked: {
			const char* path = "";
			msg->FindString("path", &path);
			if (path[0]) _LoadSessionFromPath(path);
			break;
		}

		case kMsgScheduleDialog: {
			BRect frame(Frame());
			frame.InsetBy(80, 60);
			auto* dialog = new ScheduleDialog(frame);
			dialog->CenterIn(Frame());
			dialog->Show();
			break;
		}

		case kMsgRoutineDialog: {
			BRect frame(Frame());
			frame.InsetBy(60, 40);
			auto* conv = _ActiveConv();
			auto* dialog = new RoutineDialog(frame,
				conv ? conv->GetProvider() : fProvider,
				conv ? &const_cast<Config&>(conv->GetConfig()) : fConfig,
				fTools);
			dialog->CenterIn(Frame());
			dialog->Show();
			break;
		}

		case kMsgRunRoutine: {
			if (_ActiveConv()->IsBusy()) break;
			const char* name = nullptr;
			if (msg->FindString("name", &name) != B_OK || !name) break;
			auto routines = pluto::ListRoutines();
			for (const auto& r : routines) {
				if (r.name == name) {
					std::string prompt = pluto::BuildRoutinePrompt(r);
					fInput->SetText(prompt.c_str());
					_SendCurrentInput();
					break;
				}
			}
			break;
		}

		case kMsgAgentDialog: {
			BRect frame(Frame());
			frame.InsetBy(30, 40);
			auto* dialog = new AgentStoreDialog(frame);
			dialog->CenterIn(Frame());
			dialog->Show();
			break;
		}

		case kMsgOAuthLogin: {
			// Disable the menu item and start OAuth in a background thread.
			_Append("Opening browser for Claude login…\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			thread_id tid = spawn_thread(
				[](void* data) -> int32 {
					auto* self = static_cast<MainWindow*>(data);
					bool ok = pluto::OAuthLogin();
					BMessage done(kMsgOAuthDone);
					done.AddBool("ok", ok);
					BMessenger(self).SendMessage(&done);
					return 0;
				}, "OAuthLogin", B_NORMAL_PRIORITY, this);
			if (tid >= 0) resume_thread(tid);
			break;
		}

		case kMsgOAuthLogout: {
			pluto::DeleteOAuthTokens();
			_Append("Logged out from Claude account.\n"
				"Restart Pluto to use API key.\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
			break;
		}

		case kMsgOAuthDone: {
			bool ok = false;
			msg->FindBool("ok", &ok);
			if (ok) {
				// Switch active tab to Anthropic provider with OAuth.
				pluto::OAuthTokens tokens = pluto::LoadOAuthTokens();
				if (!tokens.empty() && _ActiveConv()) {
					Config& tc = _ActiveConv()->MutableConfig();
					tc.provider = "anthropic";
					if (tc.model.find("claude") == std::string::npos)
						tc.model = "claude-sonnet-4-6";
					// Update global config too (for new tabs).
					fConfig->provider = "anthropic";
					if (fConfig->model.find("claude") == std::string::npos)
						fConfig->model = "claude-sonnet-4-6";
					pluto::SaveConfig(*fConfig);
					_ActiveConv()->SetProvider(
						std::make_unique<pluto::AnthropicProvider>(
							tokens.access_token, pluto::AuthType::OAUTH));

					_Append("\xe2\x9c\x93 Login successful! "
						"Switched to Claude (Sonnet 4.6).\n"
						"Click the status bar to change model.\n\n",
						ui_color(B_CONTROL_TEXT_COLOR));
					_UpdateTitle();
					_UpdateStatus();
				}
			} else {
				_Append("Login failed — check browser or network.\n\n",
					rgb_color{245, 101, 101, 255});
			}
			break;
		}

		case kMsgModelQuickSwitch: {
			// If the message carries a model ID, it's a selection from the
			// picker dialog — apply it to the active tab only.
			const char* picked_id = nullptr;
			if (msg->FindString("model", &picked_id) == B_OK
			    && picked_id && *picked_id && _ActiveConv()) {
				// Update active tab's config and provider.
				Config& tc = _ActiveConv()->MutableConfig();
				tc.model = picked_id;

				AutoDetectProvider(tc);

				// Auto-detect context window for the new model.
				int guessed_ctx = pluto::GuessContextWindow(tc.provider, tc.base_url, tc.model);
				if (guessed_ctx > 0) tc.context_window = guessed_ctx;

				// Picker may carry overrides from the model catalog.
				int32 mctx = 0, mmt = 0;
				if (msg->FindInt32("context_window", &mctx) == B_OK && mctx > 0)
					tc.context_window = mctx;
				if (msg->FindInt32("max_tokens", &mmt) == B_OK && mmt > 0)
					tc.max_tokens = mmt;

				// Safety cap: max_tokens must not exceed the model's
				// output limit or ~75% of context, whichever is smaller.
				int out_limit = pluto::GuessMaxOutputTokens(tc.provider, tc.base_url, tc.model);
				int ctx_cap = tc.context_window * 3 / 4;
				int cap = std::min(out_limit, ctx_cap);
				if (tc.max_tokens > cap)
					tc.max_tokens = std::max(4096, cap);

				auto p = MakeProviderForConfig(tc);
				if (p) _ActiveConv()->SetProvider(std::move(p));

				// Sync tab config → global config (for new tabs + persistence).
				fConfig->model = tc.model;
				fConfig->provider = tc.provider;
				fConfig->base_url = tc.base_url;
				fConfig->api_key_env = tc.api_key_env;
				fConfig->max_tokens = tc.max_tokens;
				fConfig->context_window = tc.context_window;
				pluto::SaveConfig(*fConfig);

				_Append("Model switched to: "
					+ std::string(picked_id) + "\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
				_UpdateTitle();
				_UpdateStatus();
				break;
			}

			// No model in the message — open the picker dialog.
			_OpenModelPicker();
			break;
		}

		case kMsgPeerMenu:
		{
			// Show a dialog listing running Pluto peers with their info.
			auto peers = ipc::find_peers();

			// Filter out ourselves.
			team_id self = -1;
			if (be_app) {
				app_info selfInfo;
				if (be_app->GetAppInfo(&selfInfo) == B_OK)
					self = selfInfo.team;
			}

			BString body;
			body << "Running Pluto instances:\n\n";
			int count = 0;
			for (const auto& p : peers) {
				if (p.team_id == self) continue;
				++count;
				body << "  PID " << (int32)p.team_id;
				if (!p.workspace.empty()) {
					body << "  —  ";
					body << p.workspace.c_str();
				}
				body << "\n";
				body << "    Model: ";
				body << (p.model.empty() ? "?" : p.model.c_str());
				body << "  |  ";
				body << (p.busy ? "busy" : "idle");
				if (!p.session_title.empty()) {
					body << "  |  ";
					body << p.session_title.c_str();
				}
				body << "\n\n";
			}
			if (count == 0) {
				body << "  (no other instances running)\n\n";
			}
			body << "Use /ask <pid> <prompt> in the CLI to send prompts.";

			BAlert* alert = new BAlert("Pluto — Peers",
				body.String(), "OK");
			alert->Go(nullptr);
			break;
		}

		case ipc::kMsgPeerAsk:
		case ipc::kMsgPeerDelegate:
		{
			// Another Pluto instance sent us a prompt to execute.
			// Inject it into our conversation as if the user typed it.
			const char* prompt_text = nullptr;
			if (msg->FindString("data", &prompt_text) == B_OK
			    && prompt_text && *prompt_text) {
				if (_ActiveConv()->IsBusy()) {
					// Can't inject while busy — send error reply.
					BMessage reply(B_ERROR);
					reply.AddString("error", "peer is busy");
					msg->SendReply(&reply);
					break;
				}
				// Set the input text and trigger a send.
				fInput->SetText(prompt_text);
				bool is_delegate = (msg->what == ipc::kMsgPeerDelegate);
				// For /delegate, we want a BNotification when done —
				// the existing notification system already fires when the
				// window is not active, so we just ensure it's noted.
				if (is_delegate) {
					// Add a marker so on_done can send a notification even
					// if the window is focused.
					// fTurnStartedAt is now on fConv; PrepareTurn resets it.
					// Notification is forced because elapsed will be large.
				}
				_SendCurrentInput();
				// Send ack reply.
				BMessage reply(B_REPLY);
				reply.AddString("result", "prompt accepted");
				msg->SendReply(&reply);
			}
			break;
		}

		default:
			// Live theme update: Haiku posts B_COLORS_UPDATED when the user
			// changes the system color scheme. Recompute our palette and
			// update view background colors so pluto matches instantly.
			if (msg->what == B_COLORS_UPDATED) {
				_InitPalette();
				_UpdateStatus();
			}
			BWindow::MessageReceived(msg);
	}
}

void MainWindow::KeyDown(const char* bytes, int32 numBytes) {
	if (numBytes != 1) {
		BWindow::KeyDown(bytes, numBytes);
		return;
	}

	uint32 modifiers = 0;
	if (CurrentMessage())
		CurrentMessage()->FindInt32("modifiers", (int32*)&modifiers);
	bool cmd = (modifiers & B_COMMAND_KEY) != 0;
	bool ctrl = (modifiers & B_CONTROL_KEY) != 0;
	bool shift = (modifiers & B_SHIFT_KEY) != 0;

	// Global shortcuts (work regardless of focused view).
	if (cmd || ctrl) {
		switch (bytes[0]) {
			case 'n':  // Cmd/Ctrl+N — new tab
				if (!shift) _CreateTab();
				else _CreateTab();  // always new tab
				return;
			case 'w':  // Cmd/Ctrl+W — close tab
				_CloseTab();
				return;
			case 't':  // Cmd/Ctrl+T — new tab (alias)
				_CreateTab();
				return;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9': {
				// Cmd/Ctrl+1-9 — select tab by number.
				int32 tab = bytes[0] - '1';
				if (fTabView && tab < fTabView->CountTabs()) {
					fTabView->Select(tab);
				}
				return;
			}
			case '/':  // Cmd/Ctrl+/ — focus input
				if (fInput) {
					fInput->MakeFocus(true);
					// Select all if shift held.
					if (shift) fInput->SelectAll();
				}
				return;
			case ',':  // Cmd/Ctrl+, — settings
				BMessenger(this).SendMessage(static_cast<uint32>('plPr'));
				return;
			case 'q':  // Cmd/Ctrl+Q — quit
				PostMessage(B_QUIT_REQUESTED);
				return;
			case 'p':  // Cmd/Ctrl+P — print/export
				_ExportMarkdown();
				return;
			case 'f':  // Cmd/Ctrl+F — find
				_ShowFindBar();
				return;
			case 'l':  // Cmd/Ctrl+L — clear conversation
				if (_ActiveConv()) _ActiveConv()->Reset();
				return;
			case 'r':  // Cmd/Ctrl+R — reset session
				if (!shift) _ResetConversation();
				return;
			case 'm':  // Cmd/Ctrl+M — model picker
				_OpenModelPicker();
				return;
			case 'f':  // Cmd/Ctrl+F — find (already handled above)
				return;
			case '+':  // Cmd/Ctrl++ — zoom in
				fFontDelta += 1;
				fConfig->font_delta = fFontDelta;
				pluto::SaveConfig(*fConfig);
				_ApplyFontZoom();
				return;
			case '-':  // Cmd/Ctrl+- — zoom out
				fFontDelta -= 1;
				fConfig->font_delta = fFontDelta;
				pluto::SaveConfig(*fConfig);
				_ApplyFontZoom();
				return;
			case '0':  // Cmd/Ctrl+0 — zoom reset
				fFontDelta = 0;
				fConfig->font_delta = fFontDelta;
				pluto::SaveConfig(*fConfig);
				_ApplyFontZoom();
				return;
			case 's':  // Cmd/Ctrl+S — save session
				if (_ActiveConv()) _ActiveConv()->SaveSession();
				return;
		}
	}

	// F11 — full screen toggle.
	if (bytes[0] == B_FUNCTION_KEY) {
		// B_FUNCTION_KEY is a prefix; check the actual function key in
		// the message's "key" field or just toggle fullscreen.
		// For simplicity, toggle fullscreen on any F-key.
		if (IsHidden() || !IsActive()) return;
		BRect frame = Frame();
		BRect screen = BScreen().Frame();
		if (frame == screen) {
			// Restore to windowed.
			ResizeTo(1020, 720);
			MoveTo((screen.Width() - 1020) / 2, (screen.Height() - 720) / 2);
		} else {
			// Go full screen.
			MoveTo(0, 0);
			ResizeTo(screen.Width(), screen.Height());
		}
		return;
	}

	BWindow::KeyDown(bytes, numBytes);
}
	if (_ActiveConv()->IsBusy()) return;
	std::string text = trim(fInput->Text() ? fInput->Text() : "");
	if (text.empty() && fPendingImages.empty()) return;

	// Add to input history.
	static_cast<ChatInput*>(fInput)->AddToHistory(text);

	// Handle slash commands.
	if (fPendingImages.empty() && _HandleSlashCommand(text)) {
		fInput->SetText("");
		return;
	}

	_ActiveConv()->AppendText("You\n", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
	if (!text.empty())
		_Append(text + "\n", ui_color(B_DOCUMENT_TEXT_COLOR));
	// Show attached images inline in the chat.
	for (const auto& img : fPendingImages) {
		if (img.kind == "audio") {
			_Append("\xf0\x9f\x94\x8a [" + img.filename + "]", ui_color(B_DOCUMENT_TEXT_COLOR));
			_Append("  (audio not processed by model)\n", {255, 180, 0, 255});
		} else if (img.kind == "pdf") {
			_Append("\xf0\x9f\x93\x84 [" + img.filename + "]\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		} else if (!img.base64_data.empty()) {
			_ActiveConv()->InsertInlineImage(img.base64_data);
		} else {
			_Append("\xf0\x9f\x96\xbc [" + img.filename + "]\n", ui_color(B_DOCUMENT_TEXT_COLOR));
		}
	}
	_Append("\n", ui_color(B_DOCUMENT_TEXT_COLOR));

fInput->SetText("");

// Fire user_prompt hooks before turn (from tab's config).
const auto& hooks = _ActiveConv()
	? _ActiveConv()->GetConfig().hooks_user_prompt
	: fConfig->hooks_user_prompt;
pluto::RunHooksSafe(hooks, "user_prompt", text);

	// Build content blocks for the message.
	// Process @file and @image mentions in the text.
	std::vector<ContentBlock> blocks;
	if (!text.empty()) {
		pluto::build_user_content(text, blocks);
	}
	for (auto& img : fPendingImages) {
		ContentBlock ib;
		if (img.kind == "audio") {
			ib.type = ContentBlock::AUDIO;
			ib.audio_format = std::move(img.media_type);
			ib.audio_data = std::move(img.base64_data);
		} else if (img.kind == "pdf") {
			ib.type = ContentBlock::DOCUMENT;
			ib.doc_media_type = std::move(img.media_type);
			ib.doc_data = std::move(img.base64_data);
		} else {
			ib.type = ContentBlock::IMAGE;
			ib.image_media_type = std::move(img.media_type);
			ib.image_data = std::move(img.base64_data);
		}
		blocks.push_back(std::move(ib));
	}
	_ClearPendingImages();

	_ActiveConv()->SendInput(std::move(blocks));
}


void MainWindow::_ResetConversation() {
	auto* c = _ActiveConv();
	if (!c || c->IsBusy()) return;
	c->Reset();
	_ClearPendingImages();
	fCtxWarningShown = 0;
	_Append("Pluto\n", ui_color(B_CONTROL_TEXT_COLOR), B_BOLD_FACE);
	_Append("(new conversation)\n\n", ui_color(B_DOCUMENT_TEXT_COLOR));
	if (fTabView) {
		int32 sel = fTabView->Selection();
		if (sel >= 0 && fTabView->TabAt(sel)) {
			std::string rm = c->GetModel();
			if (rm.rfind("claude-", 0) == 0) rm = rm.substr(7);
			if (rm.size() > 12) rm = rm.substr(0, 9) + "\xe2\x80\xa6";
			std::string rl = "New conversation \xc2\xb7 " + rm;
			fTabView->TabAt(sel)->SetLabel(rl.c_str());
		}
	}
	_ResetSidePanel();
	_UpdateTitle();
	_UpdateStatus();
	fInput->MakeFocus(true);
}

// Build a tab label: "title · provider:model" (truncated to fit).
std::string MainWindow::_BuildTabLabel(ConversationView* conv) {
	if (!conv) return "New conversation";

	// Title part.
	std::string t = conv->GetSessionTitle();
	std::string label = t.empty() ? "New conversation" : t;
	if (label.size() > 20) label = label.substr(0, 17) + "\xe2\x80\xa6";

	// Provider short name from base_url or provider id.
	const auto& cfg = conv->GetConfig();
	std::string prov;
	if (cfg.provider == "anthropic") {
		prov = "Claude";
	} else if (cfg.base_url.find("openai.com") != std::string::npos) {
		prov = "OpenAI";
	} else if (cfg.base_url.find("openrouter.ai") != std::string::npos) {
		prov = "OR";
	} else if (cfg.base_url.find("localhost:11434") != std::string::npos) {
		prov = "Ollama";
	} else if (cfg.base_url.find("localhost:1234") != std::string::npos) {
		prov = "LMS";
	} else if (cfg.base_url.find("x.ai") != std::string::npos) {
		prov = "Grok";
	} else if (cfg.base_url.find("generativelanguage.googleapis") != std::string::npos) {
		prov = "Gemini";
	} else if (cfg.base_url.find("groq.com") != std::string::npos) {
		prov = "Groq";
	} else if (cfg.base_url.find("deepseek.com") != std::string::npos) {
		prov = "DeepSeek";
	} else if (cfg.base_url.find("cerebras.ai") != std::string::npos) {
		prov = "Cerebras";
	} else if (!cfg.base_url.empty()) {
		prov = "OAI";
	} else {
		prov = cfg.provider;
	}

	// Model short name.
	std::string m = conv->GetModel();
	if (m.rfind("claude-", 0) == 0) m = m.substr(7);
	// Strip vendor prefix (e.g. "google/gemma-3-12b-it:free" → "gemma-3-12b-it:free")
	auto slash = m.rfind('/');
	if (slash != std::string::npos) m = m.substr(slash + 1);
	if (m.size() > 14) m = m.substr(0, 11) + "\xe2\x80\xa6";

	label += " \xc2\xb7 " + prov + ":" + m;
	return label;
}

ConversationView* MainWindow::_CreateTab() {
	if (fTabView && fTabView->CountTabs() >= 10) {
		BAlert* alert = new BAlert("Pluto",
			"Maximum of 10 tabs reached.\n"
			"Close a tab before opening a new one.",
			"OK", nullptr, nullptr,
			B_WIDTH_AS_USUAL, B_INFO_ALERT);
		alert->Go(nullptr);
		return _ActiveConv();
	}

	auto provider = MakeProviderForConfig(*fConfig);
	auto* conv = new ConversationView(std::move(provider), *fConfig,
	                                  fTools, this);
	// Apply current color scheme to the new TextDocumentView.
	if (conv->DocView()) {
		if (gColorScheme == "dark" || gColorScheme == "light") {
			conv->DocView()->SetViewUIColor(B_NO_COLOR);
			conv->DocView()->SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
			conv->DocView()->SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
		} else {
			conv->DocView()->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
			conv->DocView()->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		}
	}
	fTabView->AddTab(conv);
	int32 idx = fTabView->CountTabs() - 1;
	fTabView->TabAt(idx)->SetLabel(_BuildTabLabel(conv).c_str());
	fTabView->Select(idx);
	_ApplyViewColors();
	if (fFontDelta != 0) conv->ApplyFontDelta(fFontDelta);
	return conv;
}

void MainWindow::_CloseTab() {
	if (!fTabView || fTabView->CountTabs() <= 1) {
		// Last tab — just reset it instead of closing.
		_ResetConversation();
		return;
	}

	auto* c = _ActiveConv();
	if (c && c->IsBusy()) {
		BAlert* alert = new BAlert("Pluto",
			"This tab has a turn in progress.\nCancel it first.",
			"OK", nullptr, nullptr,
			B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go(nullptr);
		return;
	}

	// Confirm if conversation has content.
	if (c && !c->Messages().empty()) {
		BAlert* alert = new BAlert("Close tab",
			"Close this conversation tab?\nUnsaved content will be lost.",
			"Cancel", "Close", nullptr,
			B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		if (alert->Go() == 0) return;
	}

	int32 sel = fTabView->Selection();
	// RemoveTab returns the detached BTab. ~BTab() deletes its view,
	// so we must NOT delete `c` separately (that would be a double-free).
	BTab* tab = fTabView->RemoveTab(sel);
	delete tab;

	// Select neighbor.
	if (sel >= fTabView->CountTabs()) sel = fTabView->CountTabs() - 1;
	fTabView->Select(sel);
}

void MainWindow::_CloseOtherTabs(int32 keep) {
	if (!fTabView) return;

	// Check if any other tab is busy.
	for (int32 i = 0; i < fTabView->CountTabs(); ++i) {
		if (i == keep) continue;
		auto* conv = dynamic_cast<ConversationView*>(fTabView->ViewForTab(i));
		if (conv && conv->IsBusy()) {
			BAlert* alert = new BAlert("Pluto",
				"Another tab has a turn in progress.\n"
				"Cancel it before closing.",
				"OK", nullptr, nullptr,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			alert->Go(nullptr);
			return;
		}
	}

	// Remove tabs from the end backwards to avoid index shifting.
	// ~BTab() deletes the view, so no separate delete needed.
	for (int32 i = fTabView->CountTabs() - 1; i >= 0; --i) {
		if (i == keep) continue;
		BTab* tab = fTabView->RemoveTab(i);
		delete tab;
		// Adjust keep index if a tab before it was removed.
		if (i < keep) --keep;
	}
	fTabView->Select(0);
	_OnTabChanged();
}

// Tiny modal dialog with a BTextControl for renaming a tab.
class RenameDialog : public BWindow {
public:
	RenameDialog(BRect frame, const char* initial)
		: BWindow(frame, "Rename tab", B_MODAL_WINDOW_LOOK,
		          B_MODAL_APP_WINDOW_FEEL,
		          B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
		  fResult(-1) {
		fSem = create_sem(0, "rename-dialog");
		fInput = new BTextControl("title", "Title:", initial, nullptr);
		fInput->SetModificationMessage(new BMessage('titl'));
		auto* ok = new BButton("ok", "Rename", new BMessage('okBt'));
		auto* cancel = new BButton("cancel", "Cancel", new BMessage('caBt'));
		ok->MakeDefault(true);
		BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
			.SetInsets(12)
			.Add(fInput)
			.AddGroup(B_HORIZONTAL, 8)
				.AddGlue()
				.Add(cancel)
				.Add(ok)
			.End();
		fInput->MakeFocus(true);
		fInput->TextView()->SelectAll();
		CenterOnScreen();
	}
	~RenameDialog() override { delete_sem(fSem); }

	void MessageReceived(BMessage* msg) override {
		if (msg->what == 'okBt')   { fResult = 1; PostMessage(B_QUIT_REQUESTED); }
		else if (msg->what == 'caBt') { fResult = 0; PostMessage(B_QUIT_REQUESTED); }
		else BWindow::MessageReceived(msg);
	}
	bool QuitRequested() override { release_sem(fSem); return true; }

	// Block until the user clicks OK or Cancel. Returns the entered text
	// or empty if cancelled. Must be called from a thread other than the
	// dialog's own looper (e.g. the parent window's thread).
	std::string WaitForResult() {
		Show();
		while (acquire_sem(fSem) == B_INTERRUPTED) {}
		std::string t;
		if (fResult == 1 && LockWithTimeout(500000) == B_OK) {
			t = fInput->Text();
			Unlock();
		}
		return t;
	}

private:
	BTextControl* fInput;
	sem_id        fSem;
	int32         fResult;
};

void MainWindow::_RenameTab(int32 idx) {
	if (!fTabView || idx < 0 || idx >= fTabView->CountTabs()) return;

	auto* conv = dynamic_cast<ConversationView*>(fTabView->ViewForTab(idx));
	std::string current = conv ? conv->GetSessionTitle() : "";
	if (current.empty()) current = "New conversation";

	auto* dlg = new RenameDialog(BRect(0, 0, 320, 80), current.c_str());
	std::string title = dlg->WaitForResult();

	if (title.empty() || title == current) return;

	if (conv) conv->SetSessionTitle(title);
	fTabView->TabAt(idx)->SetLabel(_BuildTabLabel(conv).c_str());
	fTabView->Invalidate();
	if (idx == fTabView->Selection()) _UpdateTitle();
}

// Return the workspace.json path: per-project if workspace is set,
// otherwise global fallback.


void MainWindow::_OnTabChanged() {
	// Save current tab's preview bitmap before switching.
	if (fPrevConv) {
		BBitmap* saved = nullptr;
		if (fImagePreview->HasBitmap())
			saved = new BBitmap(fImagePreview->Bitmap());
		fPrevConv->SetPreviewBitmap(saved);
	}

	_ResetSidePanel();
	_UpdateTitle();
	_UpdateStatus();
	_UpdateMiniMap();
	_ClearPendingImages();

	// Restore new tab's preview bitmap.
	auto* conv = _ActiveConv();
	if (conv && conv->PreviewBitmap()) {
		fImagePreview->SetBitmap(new BBitmap(conv->PreviewBitmap()));
		if (fImagePreview->IsHidden())
			fImagePreview->Show();
	} else {
		fImagePreview->SetBitmap(nullptr);
		if (!fImagePreview->IsHidden())
			fImagePreview->Hide();
	}
	fPrevConv = conv;

	if (fInput) fInput->MakeFocus(true);
}

void MainWindow::_UpdateMiniMap() {
	if (!fMiniMap) return;
	auto* conv = _ActiveConv();
	if (!conv) { fMiniMap->SetSegments({}); return; }

	const auto& msgs = conv->Messages();
	if (msgs.empty()) { fMiniMap->SetSegments({}); return; }

	// Build segments. Each message is proportional to its text length.
	size_t total_chars = 0;
	for (const auto& m : msgs) {
		for (const auto& b : m.content)
			total_chars += b.text.size() + b.tool_input.size() + 1;
	}
	if (total_chars == 0) total_chars = 1;

	// Estimate document height from TextDocumentView layout.
	TextDocumentView* dv = conv->DocView();
	float text_height = dv ? dv->MinSize().height : 1.0f;
	if (text_height <= 0) text_height = 1.0f;

	std::vector<gui::MiniMapView::Segment> segs;
	float pos = 0.0f;
	for (const auto& m : msgs) {
		size_t msg_chars = 0;
		for (const auto& b : m.content)
			msg_chars += b.text.size() + b.tool_input.size() + 1;
		float len = static_cast<float>(msg_chars) / total_chars;

		std::string role = m.role;
		// Check if it's a tool_result message.
		for (const auto& b : m.content) {
			if (b.type == pluto::ContentBlock::TOOL_RESULT) {
				role = "tool";
				break;
			}
		}

		gui::MiniMapView::Segment seg;
		seg.role = role;
		seg.start = pos;
		seg.length = len;
		seg.scroll_y = pos * text_height;
		segs.push_back(seg);
		pos += len;
	}

	fMiniMap->SetTarget(dv, dv ? dv->ScrollBar(B_VERTICAL) : nullptr);
	fMiniMap->SetSegments(segs);
}

void MainWindow::_InitPalette() {
	gColorScheme = fConfig->color_scheme;
	bool dark = is_dark_theme();

	fprintf(stderr, "PALETTE: color_scheme='%s' dark=%d\n",
		gColorScheme.c_str(), dark);

	// Initialize native Haiku palette from system ui_color().
	pluto::gui::InitNativePalette();

	// Apply background colors chat views using the native palette.
	_ApplyViewColors();
}

void MainWindow::_ApplyViewColors() {
	// Guard: called from _InitPalette may run before views exist.
	if (!fTabView || fTabView->CountTabs() == 0) return;

	// Use native Haiku palette.
	rgb_color bg = gPalette.window_bg;
	rgb_color fg = gPalette.text_fg;
	rgb_color code_bg = gPalette.panel_bg;
	rgb_color link_color = gPalette.link_color;

	// Apply to ALL tabs.
	for (int32 i = 0; i < fTabView->CountTabs(); ++i) {
		auto* conv = static_cast<ConversationView*>(
			fTabView->ViewForTab(i));
		if (conv) {
			conv->UpdateParserColors(true, fg, code_bg, link_color);
			if (conv->DocView()) {
				conv->DocView()->SetViewUIColor(gPalette.window_bg);
				conv->DocView()->SetLowUIColor(gPalette.window_bg);
				conv->DocView()->Invalidate();
			}
		}
	}

	if (fSideView) {
		fSideView->SetViewUIColor(gPalette.window_bg);
		fSideView->SetLowUIColor(gPalette.window_bg);
		fSideView->SetHighColor(fg);
	}
	if (fInput) {
		fInput->SetViewUIColor(gPalette.window_bg);
		fInput->SetLowUIColor(gPalette.window_bg);
		BFont font;
		fInput->GetFontAndColor(0, &font);
		fInput->SetFontAndColor(&font, B_FONT_ALL, &fg);
	}
	if (fStatus)
		fStatus->SetHighColor(gPalette.text_fg);
	if (fSideTitle)
		fSideTitle->SetHighColor(gPalette.text_fg);
}

void MainWindow::_RenderMarkdown(const std::string& text) {
	fSideView->SetText("");
	fSideLinks.clear();
	fSideAnchors.clear();

	// Use forced text color in dark/light mode, system color in auto.
	rgb_color body_col    = (gColorScheme == "dark")  ? ui_color(B_DOCUMENT_TEXT_COLOR)
	                      : (gColorScheme == "light") ? ui_color(B_DOCUMENT_TEXT_COLOR)
	                      : ui_color(B_DOCUMENT_TEXT_COLOR);
	rgb_color dim_col     = ui_color(B_DOCUMENT_TEXT_COLOR);
	rgb_color code_col    = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color heading_col = ui_color(B_CONTROL_TEXT_COLOR);
	rgb_color link_col    = ui_color(B_CONTROL_TEXT_COLOR);
	rgb_color check_col   = ui_color(B_CONTROL_TEXT_COLOR);

	auto append = [this](const std::string& s, rgb_color c,
	                      uint16 face, bool mono, int size_delta) {
		if (s.empty()) return;
		int32 start = fSideView->TextLength();
		fSideView->Insert(start, s.data(), s.size());
		int32 end = fSideView->TextLength();
		BFont f = mono ? *be_fixed_font : *be_plain_font;
		if (face != 0) f.SetFace(face);
		f.SetSize(f.Size() + fFontDelta + size_delta);
		fSideView->SetFontAndColor(start, end, &f, B_FONT_ALL, &c);
	};

	// Render a line with inline formatting: **bold**, *italic*, `code`,
	// [link](url), ~~strike~~
	auto render_inline = [&](const std::string& line) {
		size_t i = 0;
		while (i < line.size()) {
			// **bold**
			if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
				size_t end = line.find("**", i + 2);
				if (end != std::string::npos) {
					append(line.substr(i + 2, end - i - 2),
						body_col, B_BOLD_FACE, false, 0);
					i = end + 2;
					continue;
				}
			}
			// *italic* (single asterisk, not preceded by another *)
			if (line[i] == '*' && (i == 0 || line[i - 1] != '*')
			    && i + 1 < line.size() && line[i + 1] != '*') {
				size_t end = line.find('*', i + 1);
				if (end != std::string::npos && (end + 1 >= line.size()
				    || line[end + 1] != '*')) {
					append(line.substr(i + 1, end - i - 1),
						body_col, B_ITALIC_FACE, false, 0);
					i = end + 1;
					continue;
				}
			}
			// `inline code` — monospace with visual padding
			if (line[i] == '`') {
				size_t end = line.find('`', i + 1);
				if (end != std::string::npos) {
					append("\xC2\xA0", code_col, 0, true, -1);  // NBSP padding
					append(line.substr(i + 1, end - i - 1),
						code_col, 0, true, -1);
					append("\xC2\xA0", code_col, 0, true, -1);
					i = end + 1;
					continue;
				}
			}
			// [text](url)
			if (line[i] == '[') {
				size_t close = line.find(']', i + 1);
				if (close != std::string::npos && close + 1 < line.size()
				    && line[close + 1] == '(') {
					size_t pclose = line.find(')', close + 2);
					if (pclose != std::string::npos) {
						std::string label = line.substr(i + 1, close - i - 1);
						std::string url = line.substr(close + 2,
							pclose - close - 2);
						// Render as underlined blue text, record clickable region.
						int32 lk_start = fSideView->TextLength();
						append(label, link_col, B_UNDERSCORE_FACE, false, 0);
						int32 lk_end = fSideView->TextLength();
						fSideLinks.push_back({lk_start, lk_end, url});
						// Show URL after label only for external links.
						if (!url.empty() && url[0] != '#' && url != label) {
							append(" (" + url + ")", dim_col, 0, false, -1);
						}
						i = pclose + 1;
						continue;
					}
				}
			}
			// ~~strikethrough~~
			if (i + 1 < line.size() && line[i] == '~' && line[i + 1] == '~') {
				size_t end = line.find("~~", i + 2);
				if (end != std::string::npos) {
					append(line.substr(i + 2, end - i - 2),
						dim_col, B_STRIKEOUT_FACE, false, 0);
					i = end + 2;
					continue;
				}
			}
			// Plain text — accumulate until next special char
			size_t next = i + 1;
			while (next < line.size()
			    && line[next] != '*' && line[next] != '`'
			    && line[next] != '[' && line[next] != '~')
				++next;
			append(line.substr(i, next - i), body_col, 0, false, 0);
			i = next;
		}
	};

	bool in_code_block = false;
	std::string code_lang;
	std::string code_accum;

	// Table accumulator for two-pass rendering.
	struct TableRow {
		std::vector<std::string> cells;
		bool is_separator = false;
	};
	std::vector<TableRow> table_rows;

	auto parse_table_cells = [](const std::string& raw) -> TableRow {
		// Pre-process: replace \| with placeholder so escaped pipes
		// aren't treated as cell separators.
		std::string ln;
		ln.reserve(raw.size());
		for (size_t i = 0; i < raw.size(); ++i) {
			if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '|') {
				ln += '\x01';
				++i;
			} else {
				ln += raw[i];
			}
		}

		TableRow tr;
		size_t pos = 1;
		while (pos < ln.size()) {
			size_t pipe = ln.find('|', pos);
			if (pipe == std::string::npos) break;
			std::string cell = ln.substr(pos, pipe - pos);
			// Restore escaped pipes
			for (auto& ch : cell) if (ch == '\x01') ch = '|';
			while (!cell.empty() && cell.front() == ' ') cell.erase(0, 1);
			while (!cell.empty() && cell.back() == ' ') cell.pop_back();
			tr.cells.push_back(cell);
			pos = pipe + 1;
		}
		if (!tr.cells.empty()) {
			tr.is_separator = true;
			for (const auto& c : tr.cells) {
				for (char ch : c)
					if (ch != '-' && ch != ':' && ch != ' ')
						{ tr.is_separator = false; break; }
				if (!tr.is_separator) break;
			}
		}
		return tr;
	};

	auto flush_table = [&]() {
		if (table_rows.empty()) return;
		size_t ncols = 0;
		for (const auto& r : table_rows)
			if (r.cells.size() > ncols) ncols = r.cells.size();
		std::vector<size_t> widths(ncols, 3);
		for (const auto& r : table_rows) {
			if (r.is_separator) continue;
			for (size_t ci = 0; ci < r.cells.size() && ci < ncols; ++ci)
				if (r.cells[ci].size() > widths[ci])
					widths[ci] = r.cells[ci].size();
		}

		auto make_sep = [&]() -> std::string {
			std::string s = "  ";
			for (size_t ci = 0; ci < ncols; ++ci) {
				for (size_t j = 0; j < widths[ci] + 2; ++j)
					s += "\xe2\x94\x80";
				if (ci + 1 < ncols) s += "\xe2\x94\xbc";
			}
			return s + "\n";
		};
		auto make_data = [&](const std::vector<std::string>& cells) -> std::string {
			std::string s = "  ";
			for (size_t ci = 0; ci < ncols; ++ci) {
				std::string v = ci < cells.size() ? cells[ci] : "";
				while (v.size() < widths[ci]) v += ' ';
				s += " "; s += v; s += " ";
				if (ci + 1 < ncols) s += "\xe2\x94\x82";
			}
			return s + "\n";
		};

		append("\n", body_col, 0, false, 0);
		bool header_done = false;
		for (const auto& r : table_rows) {
			if (r.is_separator) {
				append(make_sep(), dim_col, 0, true, 0);
			} else if (!header_done) {
				append(make_data(r.cells), heading_col, B_BOLD_FACE, true, 0);
				header_done = true;
			} else {
				append(make_data(r.cells), body_col, 0, true, 0);
			}
		}
		append("\n", body_col, 0, false, 0);
		table_rows.clear();
	};

	std::istringstream stream(text);
	std::string line;
	while (std::getline(stream, line)) {
		while (!line.empty() && line.back() == '\r') line.pop_back();

		// Fenced code block
		if (line.size() >= 3 && line.substr(0, 3) == "```") {
			if (!table_rows.empty()) flush_table();
			if (in_code_block) {
				// Code block header with decorative border.
				{
					std::string header = "  \xE2\x94\x8C\xE2\x94\x80 ";  // ┌─
					header += code_lang.empty() ? "code" : code_lang;
					header += " \xE2\x94\x80\xE2\x94\x80"
					          "\xE2\x94\x80\xE2\x94\x80"
					          "\xE2\x94\x80\xE2\x94\x80\n";  // ──────
					append(header, dim_col, 0, true, -2);
				}

				// Syntax-highlighted code block.
				auto syn_tokens = pluto::gui::highlight(code_accum, code_lang);
				for (const auto& tok : syn_tokens) {
					rgb_color tc = code_col;
					switch (tok.type) {
						case pluto::gui::TokenType::Keyword:
							tc = ui_color(B_CONTROL_TEXT_COLOR); break;
						case pluto::gui::TokenType::String:
							tc = ui_color(B_CONTROL_TEXT_COLOR); break;
						case pluto::gui::TokenType::Comment:
							tc = ui_color(B_DOCUMENT_TEXT_COLOR); break;
						case pluto::gui::TokenType::Number:
							tc = ui_color(B_CONTROL_TEXT_COLOR); break;
						case pluto::gui::TokenType::Preprocessor:
							tc = ui_color(B_CONTROL_TEXT_COLOR); break;
						case pluto::gui::TokenType::Type:
							tc = ui_color(B_CONTROL_TEXT_COLOR); break;
						default: break;
					}
					append(tok.text, tc, 0, true, 0);
				}
				// Code block footer.
				append("  \xE2\x94\x94\xE2\x94\x80"
				       "\xE2\x94\x80\xE2\x94\x80"
				       "\xE2\x94\x80\xE2\x94\x80"
				       "\xE2\x94\x80\xE2\x94\x80\n\n",
				       dim_col, 0, true, -2);  // └────────
				code_accum.clear();
				code_lang.clear();
				in_code_block = false;
			} else {
				code_lang = line.size() > 3 ? line.substr(3) : "";
				while (!code_lang.empty() && code_lang.front() == ' ')
					code_lang.erase(0, 1);
				in_code_block = true;
			}
			continue;
		}
		if (in_code_block) {
			code_accum += "  " + line + "\n";
			continue;
		}

		// Table row (starts with |) — accumulate for two-pass rendering.
		if (!line.empty() && line[0] == '|') {
			TableRow tr = parse_table_cells(line);
			if (!tr.cells.empty())
				table_rows.push_back(std::move(tr));
			continue;
		}

		// Non-table line: flush any accumulated table first.
		if (!table_rows.empty()) flush_table();

		// Explicit HTML anchor tag: <a id="..."></a>
		if (line.find("<a id=\"") == 0) {
			size_t q1 = 7;
			size_t q2 = line.find('"', q1);
			if (q2 != std::string::npos) {
				fSideAnchors[line.substr(q1, q2 - q1)] =
					fSideView->TextLength();
			}
			continue;  // don't render the raw HTML tag
		}

		// Headings
		if (!line.empty() && line[0] == '#') {
			int level = 0;
			while (level < (int)line.size() && line[level] == '#') ++level;
			std::string heading = line.substr(level);
			while (!heading.empty() && heading.front() == ' ')
				heading.erase(0, 1);

			// Compute GFM-style anchor ID and record as target.
			{
				std::string anchor;
				for (char c : heading) {
					unsigned char u = static_cast<unsigned char>(c);
					if (std::isalnum(u))
						anchor += static_cast<char>(std::tolower(u));
					else if (c == ' ' || c == '-')
						anchor += '-';
				}
				while (!anchor.empty() && anchor.back() == '-')
					anchor.pop_back();
				if (!anchor.empty())
					fSideAnchors[anchor] = fSideView->TextLength();
			}

			int size_d = (level == 1) ? 6 : (level == 2) ? 3 : 1;
			if (level == 1) {
				// Draw overline for H1
				append("\n", body_col, 0, false, 0);
			}
			append(heading + "\n", heading_col, B_BOLD_FACE, false, size_d);
			if (level <= 2) {
				// Underline for H1/H2
				size_t ul = heading.size();
				if (ul > 40) ul = 40;
				std::string underline;
				for (size_t j = 0; j < ul; ++j)
					underline += (level == 1) ? "\xe2\x94\x81" : "\xe2\x94\x80"; // ━ / ─
				append(underline + "\n", dim_col, 0, true, 0);
			}
			append("\n", body_col, 0, false, 0);
			continue;
		}

		// Horizontal rule
		if (line.size() >= 3 && (line.find("---") == 0 || line.find("***") == 0)) {
			bool is_rule = true;
			for (char c : line)
				if (c != '-' && c != '*' && c != ' ') { is_rule = false; break; }
			if (is_rule) {
				append("\n  \xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
				       "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
				       "\xE2\x94\x80 \xE2\x94\x80 \xE2\x94\x80 "
				       "\xE2\x94\x80\n\n", dim_col, 0, true, 0);
				continue;
			}
		}

		// Blockquote — thick colored bar + italic
		if (!line.empty() && line[0] == '>') {
			std::string q = line.substr(1);
			while (!q.empty() && q.front() == ' ') q.erase(0, 1);
			append("   \xE2\x94\x83 ", heading_col, 0, false, 0); // ┃
			append(q, dim_col, B_ITALIC_FACE, false, 0);
			append("\n", dim_col, 0, false, 0);
			continue;
		}

		// Task list  - [ ] or - [x]
		if (line.size() >= 5 && line[0] == '-' && line[1] == ' '
		    && line[2] == '[') {
			bool checked = (line[3] == 'x' || line[3] == 'X');
			std::string item = line.substr(5);
			while (!item.empty() && item.front() == ' ') item.erase(0, 1);
			const char* box = checked ? "  \xe2\x98\x91 " : "  \xe2\x98\x90 "; // ☑ / ☐
			append(box, checked ? check_col : dim_col, 0, false, 0);
			if (checked)
				append(item, dim_col, B_STRIKEOUT_FACE, false, 0);
			else
				render_inline(item);
			append("\n", body_col, 0, false, 0);
			continue;
		}

		// Bullet list — dimmed bullet, wider indent
		if (line.size() >= 2 && (line[0] == '-' || line[0] == '*')
		    && line[1] == ' ') {
			append("   \xE2\x80\xA2 ", dim_col, 0, false, 0); // •
			render_inline(line.substr(2));
			append("\n", body_col, 0, false, 0);
			continue;
		}

		// Numbered list
		if (line.size() >= 3 && std::isdigit((unsigned char)line[0])) {
			size_t dot = line.find(". ");
			if (dot != std::string::npos && dot <= 3) {
				append("  " + line.substr(0, dot + 2),
					body_col, B_BOLD_FACE, false, 0);
				render_inline(line.substr(dot + 2));
				append("\n", body_col, 0, false, 0);
				continue;
			}
		}

		// Empty line
		if (line.empty()) {
			append("\n", body_col, 0, false, 0);
			continue;
		}

		// Normal paragraph with inline formatting
		render_inline(line);
		append("\n", body_col, 0, false, 0);
	}

	// Flush pending state
	if (!table_rows.empty()) flush_table();
	if (!code_accum.empty()) {
		if (!code_lang.empty())
			append("  [" + code_lang + "]\n", dim_col, 0, true, -2);
		auto syn_tokens = pluto::gui::highlight(code_accum, code_lang);
		for (const auto& tok : syn_tokens) {
			rgb_color tc = code_col;
			switch (tok.type) {
				case pluto::gui::TokenType::Keyword:
					tc = ui_color(B_CONTROL_TEXT_COLOR); break;
				case pluto::gui::TokenType::String:
					tc = ui_color(B_CONTROL_TEXT_COLOR); break;
				case pluto::gui::TokenType::Comment:
					tc = ui_color(B_DOCUMENT_TEXT_COLOR); break;
				case pluto::gui::TokenType::Number:
					tc = ui_color(B_CONTROL_TEXT_COLOR); break;
				case pluto::gui::TokenType::Preprocessor:
					tc = ui_color(B_CONTROL_TEXT_COLOR); break;
				case pluto::gui::TokenType::Type:
					tc = ui_color(B_CONTROL_TEXT_COLOR); break;
				default: break;
			}
			append(tok.text, tc, 0, true, 0);
		}
	}

	fSideView->ScrollToOffset(0);
}

void MainWindow::_RenderEditDiff(const std::string& input_json) {
	std::string path, old_s, new_s;
	try {
		auto v = pluto::json::Value::parse(input_json);
		path  = v.str("path");
		old_s = v.str("old_string");
		new_s = v.str("new_string");
	} catch (const std::exception&) {
		_UpdateSidePanel("edit_file", input_json, false);
		return;
	}

	BString title;
	title.SetToFormat("edit_file  %s", path.c_str());
	fSideTitle->SetText(title.String());

	fSideView->SetText("");

	auto append_block = [this](const std::string& text,
	                           rgb_color color, char marker) {
		size_t pos = 0;
		while (pos < text.size()) {
			size_t nl = text.find('\n', pos);
			std::string line(1, marker);
			line += " ";
			line += text.substr(pos,
				nl == std::string::npos ? text.size() - pos : nl - pos);
			line += "\n";
			int32 s = fSideView->TextLength();
			fSideView->Insert(s, line.data(), line.size());
			BFont f = *be_fixed_font;
			fSideView->SetFontAndColor(s, fSideView->TextLength(),
				&f, B_FONT_ALL, &color);
			if (nl == std::string::npos) break;
			pos = nl + 1;
		}
	};

	rgb_color red   = { 200,  50,  50, 255 };
	rgb_color green = {  30, 140,  70, 255 };

	if (!old_s.empty()) append_block(old_s, red,   '-');
	if (!new_s.empty()) append_block(new_s, green, '+');

	fSideView->ScrollToOffset(0);
}

// _BuildMarkdown has been moved to ConversationView.

void MainWindow::_UpdateTitle(const char* activity) {
	BString title("Pluto");

	// Workspace with lock (skip if leaf == "pluto" to avoid "Pluto — pluto")
	std::string ws = pluto::WorkspaceRoot();
	if (!ws.empty()) {
		bool trusted = pluto::IsWorkspaceTrusted(ws)
		            || pluto::IsSessionTrusted();
		const char* trust_tag = trusted ? " [trusted]" : " [untrusted]";
		size_t slash = ws.rfind('/');
		std::string leaf = slash != std::string::npos
			? ws.substr(slash + 1) : ws;
		if (leaf != "pluto") {
			title += " — ";
			title += leaf.c_str();
			title += trust_tag;
		} else {
			title += trust_tag;
		}
	}

	// Model from active tab (shortened: strip "claude-" prefix for brevity)
	std::string model = _ActiveConv()
		? _ActiveConv()->GetModel() : fConfig->model;
	if (model.rfind("claude-", 0) == 0) model = model.substr(7);
	title += " — ";
	title += model.c_str();

	// Session title
	std::string sessTitle = _ActiveConv() ? _ActiveConv()->GetSessionTitle() : std::string();
	if (!sessTitle.empty()) {
		title += " — ";
		title += sessTitle.c_str();
	}

	// Dynamic activity
	if (activity && *activity) {
		title += " — ";
		title += activity;
	}

	SetTitle(title.String());
}

void MainWindow::_ShowFindBar() {
	if (fFindBar->IsHidden()) fFindBar->Show();
	fFindBar->MakeFocus(true);
	fFindOffset = 0;
}

void MainWindow::_HideFindBar() {
	if (!fFindBar->IsHidden()) fFindBar->Hide();
	fInput->MakeFocus(true);
	fFindOffset = 0;
}

void MainWindow::_FindInConversation(bool forward) {
	const char* query = fFindBar->Text();
	if (!query || !*query) return;
	_ActiveConv()->FindInConversation(std::string(query), forward);
}

void MainWindow::_SaveImageToFile(const entry_ref* dir, const char* name) {
	BBitmap* bmp = fImagePreview->Bitmap();
	if (!bmp) return;

	BDirectory d(dir);
	BFile file(&d, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK) {
		_Append("[error] Could not create file.\n", rgb_color{245, 101, 101, 255});
		return;
	}

	BBitmapStream stream(bmp);
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	status_t err = roster->Translate(&stream, nullptr, nullptr, &file,
		B_PNG_FORMAT);

	// Detach bitmap so ImagePreviewView still owns it.
	BBitmap* detached = nullptr;
	stream.DetachBitmap(&detached);

	if (err == B_OK) {
		BPath path(&d, name);
		std::string msg = "  \xe2\x9c\x93 Image saved to ";
		msg += path.Path();
		msg += "\n";
		_Append(msg, ui_color(B_DOCUMENT_TEXT_COLOR), 0, true);
	} else {
		_Append("[error] Failed to save image.\n", rgb_color{245, 101, 101, 255});
	}
}

void MainWindow::_OpenModelPicker() {
	auto* picker = new ModelPickerWindow(BMessenger(this), fConfig);
	picker->Show();
}

void MainWindow::_OpenSettings() {
	auto* dlg = new SettingsDialog(BMessenger(this), *fConfig);
	dlg->CenterOnScreen();
	dlg->Show();
}

void MainWindow::_ApplyFontZoom() {
	// Apply font zoom to ALL tabs, not just the active one.
	if (fTabView) {
		for (int32 i = 0; i < fTabView->CountTabs(); ++i) {
			auto* conv = static_cast<ConversationView*>(
				fTabView->ViewForTab(i));
			if (conv) conv->ApplyFontDelta(fFontDelta);
		}
	}
	if (fSideView && fSideView->TextLength() > 0) {
		BFont f;
		f.SetSize(be_plain_font->Size() + fFontDelta);
		fSideView->SetFontAndColor(0, fSideView->TextLength(),
			&f, B_FONT_SIZE, nullptr);
	}
}

void MainWindow::_SetSendStopMode(bool stopping) {
	if (stopping) {
		fSend->SetLabel("Stop");
		fSend->SetMessage(new BMessage(kMsgStop));
	} else {
		fSend->SetLabel("Send");
		fSend->SetMessage(new BMessage(kMsgSend));
	}
}

void MainWindow::_SaveBitmapAndAttach(BBitmap* bmp) {
	if (!bmp || !bmp->IsValid()) return;

	// Translate BBitmap → PNG via BTranslatorRoster.
	BBitmapStream stream(bmp);
	BMallocIO pngIO;
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	if (roster->Translate(&stream, nullptr, nullptr,
	    &pngIO, B_PNG_FORMAT) != B_OK) {
		stream.DetachBitmap(&bmp);  // reclaim ownership
		_Append("[error: could not encode bitmap as PNG]\n", rgb_color{245, 101, 101, 255});
		return;
	}
	stream.DetachBitmap(&bmp);  // reclaim so caller can delete

	// Save to temp file. mkstemp requires X's at the end, so we
	// create a temp file and rename it with .png extension.
	char tmp[] = "/tmp/pluto-clip-XXXXXX";
	int fd = mkstemp(tmp);
	if (fd < 0) {
		_Append("[error: could not create temp file for bitmap]\n", rgb_color{245, 101, 101, 255});
		return;
	}
	::write(fd, pngIO.Buffer(), pngIO.BufferLength());
	::close(fd);
	// Rename with .png extension so image detection works.
	std::string png_path = std::string(tmp) + ".png";
	::rename(tmp, png_path.c_str());

	_AttachImageToInput(png_path, "image/png");
}

void MainWindow::_HandleBitmapClipDrop(const void* data, ssize_t size) {
	if (!data || size <= 0) return;

	// Try to interpret raw data as a BBitmap via BMemoryIO + BTranslatorRoster.
	BMemoryIO memIO(data, static_cast<size_t>(size));
	BBitmap* bmp = nullptr;
	BBitmapStream stream;
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	if (roster->Translate(&memIO, nullptr, nullptr,
	    &stream, B_TRANSLATOR_BITMAP) == B_OK) {
		stream.DetachBitmap(&bmp);
	}
	if (bmp && bmp->IsValid()) {
		_SaveBitmapAndAttach(bmp);
		delete bmp;
	}
}

void MainWindow::_AttachFileToInput(const std::string& path) {
	std::string current = fInput->Text() ? fInput->Text() : "";
	if (!current.empty() && current.back() != '\n') current += "\n";
	current += "@" + path;
	fInput->SetText(current.c_str());
}

void MainWindow::_AttachImageToInput(const std::string& path,
                                     const std::string& media_type) {
	// Read binary data and encode as base64.
	struct stat st;
	if (stat(path.c_str(), &st) != 0 || st.st_size > 20 * 1024 * 1024) {
		_Append("Image too large or unreadable: " + path + "\n", rgb_color{245, 101, 101, 255});
		return;
	}
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		_Append("Cannot read image: " + path + "\n", rgb_color{245, 101, 101, 255});
		return;
	}
	std::string raw(static_cast<size_t>(st.st_size), '\0');
	f.read(raw.data(), st.st_size);

	BPath bp(path.c_str());
	PendingImage img;
	img.path = path;
	img.filename = bp.Leaf() ? bp.Leaf() : path;
	img.media_type = media_type;
	img.base64_data = pluto::base64_encode(raw);
	fPendingImages.push_back(std::move(img));

	_UpdateImageChips();

	// Show preview in side panel immediately.
	BMallocIO memIO;
	memIO.SetSize(raw.size());
	memIO.Write(raw.data(), raw.size());
	memIO.Seek(0, SEEK_SET);
	BBitmap* bitmap = nullptr;
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	BBitmapStream stream;
	if (roster->Translate(&memIO, nullptr, nullptr,
	    &stream, B_TRANSLATOR_BITMAP) == B_OK) {
		stream.DetachBitmap(&bitmap);
	}
	if (bitmap) {
		fImagePreview->SetBitmap(bitmap);
		if (fImagePreview->IsHidden())
			fImagePreview->Show();
		// Show detailed image info in side panel.
		BString title;
		title.SetToFormat("\xf0\x9f\x96\xbc %s", img.filename.c_str());
		fSideTitle->SetText(title.String());
		BRect bounds = bitmap->Bounds();
		std::string info;
		info += "Path:   " + path + "\n";
		if (st.st_size < 1024 * 1024) {
			info += "Size:   " + std::to_string(st.st_size / 1024) + " KB\n";
		} else {
			char sbuf[32];
			snprintf(sbuf, sizeof(sbuf), "%.1f MB",
				st.st_size / (1024.0 * 1024.0));
			info += "Size:   "; info += sbuf; info += "\n";
		}
		{
			char dim[64];
			snprintf(dim, sizeof(dim), "%d x %d px",
				(int)(bounds.Width() + 1), (int)(bounds.Height() + 1));
			info += "Dimensions: "; info += dim; info += "\n";
		}
		info += "Format: " + media_type + "\n";
		fSideView->SetText(info.c_str());
	}
}

// Query file metadata via ffprobe (audio/video) or stat (PDF).
static std::string _ProbeMediaInfo(const std::string& path,
                                   const std::string& kind,
                                   off_t file_size) {
	std::string info;
	info += "Path:   " + path + "\n";

	// Human-readable size.
	if (file_size < 1024)
		info += "Size:   " + std::to_string(file_size) + " B\n";
	else if (file_size < 1024 * 1024)
		info += "Size:   " + std::to_string(file_size / 1024) + " KB\n";
	else {
		char buf[32];
		snprintf(buf, sizeof(buf), "%.1f MB", file_size / (1024.0 * 1024.0));
		info += "Size:   "; info += buf; info += "\n";
	}

	// BFS attributes (MIME type).
	{
		BNode node(path.c_str());
		if (node.InitCheck() == B_OK) {
			char mime[256];
			ssize_t n = node.ReadAttr("BEOS:TYPE", B_STRING_TYPE, 0,
			                           mime, sizeof(mime) - 1);
			if (n > 0) {
				mime[n] = '\0';
				info += "Type:   ";
				info += mime;
				info += "\n";
			}
		}
	}

	if (kind == "audio" || kind == "video") {
		// Use ffprobe for duration, codec, sample rate, channels.
		std::string cmd = "ffprobe -v quiet -print_format flat "
			"-show_format -show_streams '";
		// Escape single quotes in path.
		for (char c : path) {
			if (c == '\'') cmd += "'\\''";
			else cmd += c;
		}
		cmd += "' 2>/dev/null";
		std::string output;
		int rc = pluto::RunCommandCapture(cmd.c_str(), output);
		if (rc == 0 && !output.empty()) {
			// Parse output
			std::istringstream iss(output);
			std::string line;
			auto val = [&](const std::string& key) -> std::string {
				size_t pos = line.find(key + "=");
				if (pos == std::string::npos) return {};
				size_t start = pos + key.size() + 1;
				std::string v = line.substr(start);
				while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == '"')) v.pop_back();
				while (!v.empty() && v.front() == '"') v.erase(0, 1);
				return v;
			};
			while (std::getline(iss, line)) {
				if (duration.empty()) duration = val("format.duration");
				if (codec.empty()) codec = val("streams.stream.0.codec_long_name");
				if (sample_rate.empty()) sample_rate = val("streams.stream.0.sample_rate");
				if (channels.empty()) channels = val("streams.stream.0.channels");
				if (bit_rate.empty()) bit_rate = val("streams.stream.0.bit_rate");
			}
		}

			if (!duration.empty()) {
				double secs = std::atof(duration.c_str());
				int min = (int)secs / 60;
				int sec = (int)secs % 60;
				char tbuf[32];
				snprintf(tbuf, sizeof(tbuf), "%d:%02d", min, sec);
				info += "Duration: "; info += tbuf; info += "\n";
			}
			if (!codec.empty())
				info += "Codec:  " + codec + "\n";
			if (!sample_rate.empty())
				info += "Sample: " + sample_rate + " Hz\n";
			if (!channels.empty())
				info += "Channels: " + channels + "\n";
			if (!bit_rate.empty()) {
				int kbps = std::atoi(bit_rate.c_str()) / 1000;
				if (kbps > 0)
					info += "Bitrate: " + std::to_string(kbps) + " kbps\n";
			}
		}
	} else if (kind == "pdf") {
		// Count pages via pdfinfo if available, otherwise note.
		FILE* p = popen(("pdfinfo '" + path + "' 2>/dev/null").c_str(), "r");
		if (p) {
			char buf[256];
			while (fgets(buf, sizeof(buf), p)) {
				if (strncmp(buf, "Pages:", 6) == 0) {
					info += buf;  // "Pages:  N\n"
				} else if (strncmp(buf, "Title:", 6) == 0) {
					info += buf;
				} else if (strncmp(buf, "Author:", 7) == 0) {
					info += buf;
				} else if (strncmp(buf, "Creator:", 8) == 0) {
					info += buf;
				}
			}
			pclose(p);
		}
	}

	return info;
}

void MainWindow::_AttachMediaToInput(const std::string& path,
                                     const std::string& format,
                                     const std::string& kind) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0 || st.st_size > 20 * 1024 * 1024) {
		_Append("File too large or unreadable: " + path + "\n", rgb_color{245, 101, 101, 255});
		return;
	}
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		_Append("Cannot read file: " + path + "\n", rgb_color{245, 101, 101, 255});
		return;
	}
	std::string raw(static_cast<size_t>(st.st_size), '\0');
	f.read(raw.data(), st.st_size);

	BPath bp(path.c_str());
	PendingImage img;
	img.path = path;
	img.filename = bp.Leaf() ? bp.Leaf() : path;
	img.media_type = format;
	img.base64_data = pluto::base64_encode(raw);
	img.kind = kind;
	fPendingImages.push_back(std::move(img));

	_UpdateImageChips();

	// Show detailed file info in side panel.
	const char* icon = (kind == "audio") ? "\xf0\x9f\x94\x8a"
	                 : (kind == "pdf")   ? "\xf0\x9f\x93\x84"
	                                     : "\xf0\x9f\x93\x8e";
	BString title;
	title.SetToFormat("%s %s", icon, img.filename.c_str());
	fSideTitle->SetText(title.String());

	std::string info = _ProbeMediaInfo(path, kind, st.st_size);
	fSideView->SetText(info.c_str());

	// Show/hide mini player for audio files.
	if (kind == "audio") {
		fPlayerPath = path;
		fPlayerPlay->SetLabel("\xe2\x96\xb6");  // ▶
		if (fPlayerBar->IsHidden()) fPlayerBar->Show();
	} else {
		fPlayerPath.clear();
		if (!fPlayerBar->IsHidden()) fPlayerBar->Hide();
	}
	// Hide image preview for non-image attachments.
	if (!fImagePreview->IsHidden()) fImagePreview->Hide();
}

void MainWindow::_ClearPendingImages() {
	fPendingImages.clear();
	_UpdateImageChips();
}

void MainWindow::_UpdateImageChips() {
	// Remove all existing chip buttons.
	while (fImageChips->CountChildren() > 0) {
		BView* child = fImageChips->ChildAt(0);
		fImageChips->RemoveChild(child);
		delete child;
	}

	if (fPendingImages.empty()) {
		if (!fImageChips->IsHidden()) fImageChips->Hide();
		return;
	}

	// Add a chip button per attachment: "[icon] filename  ✕"
	for (size_t i = 0; i < fPendingImages.size(); ++i) {
		const char* icon = "\xf0\x9f\x96\xbc";  // 🖼 image default
		if (fPendingImages[i].kind == "audio")
			icon = "\xf0\x9f\x94\x8a";  // 🔊
		else if (fPendingImages[i].kind == "pdf")
			icon = "\xf0\x9f\x93\x84";  // 📄
		BString label;
		label.SetToFormat("%s %s  \xc3\x97",
			icon, fPendingImages[i].filename.c_str());
		auto* msg = new BMessage(kMsgImageRemove);
		msg->AddInt32("index", (int32)i);
		auto* btn = new BButton(nullptr, label.String(), msg);
		BFont f = *be_plain_font;
		f.SetSize(f.Size() - 2);
		btn->SetFont(&f);
		btn->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 22));
		fImageChips->AddChild(btn);
	}

	if (fImageChips->IsHidden()) fImageChips->Show();
	fImageChips->InvalidateLayout();
}

void MainWindow::_LoadImagePreviewFromBase64(const std::string& base64_data) {
	std::string raw = pluto::base64_decode(base64_data);
	if (raw.empty()) return;

	BMallocIO memIO;
	memIO.SetSize(raw.size());
	memIO.Write(raw.data(), raw.size());
	memIO.Seek(0, SEEK_SET);

	BBitmap* bitmap = nullptr;
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	BBitmapStream stream;
	if (roster->Translate(&memIO, nullptr, nullptr,
	    &stream, B_TRANSLATOR_BITMAP) == B_OK) {
		stream.DetachBitmap(&bitmap);
	}
	if (bitmap) {
		fImagePreview->SetBitmap(bitmap);
		if (fImagePreview->IsHidden())
			fImagePreview->Show();
	}
}

void MainWindow::_ShowAbout() {
	BAlert* alert = new BAlert("About Pluto",
		"Pluto\n"
		"Haiku-native Claude client.\n\n"
		"Built in C++17 with libcurl and the Be API.\n"
		"No SDK, no Electron, no regrets.\n\n"
		"A production of the\n"
		"Movement for the Liberation of Walter OS\n\n"
		"\"BeOS never died — it just waited for\n"
		" the world to catch up. Now it has AI.\"",
		"Viva Walter!");
	alert->SetShortcut(0, B_ESCAPE);
	alert->Go(nullptr);
}

}  // namespace gui
}  // namespace pluto
