#include "SessionBrowser.h"

#include <Alert.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>

#include <cctype>
#include <ctime>
#include <string>
#include <vector>

#include "../storage/SessionStore.h"

namespace pluto {
namespace gui {

namespace {

const uint32 kMsgSelect  = 'sbSl';
const uint32 kMsgFilter  = 'sbFi';
const uint32 kMsgLoad    = 'sbLd';
const uint32 kMsgDelete  = 'sbDl';
const uint32 kMsgDblClick = 'sbDc';

std::string format_date(int64_t unix_ts) {
	if (unix_ts <= 0) return "";
	std::time_t t = static_cast<std::time_t>(unix_ts);
	std::tm tm;
	if (!localtime_r(&t, &tm)) return "";
	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
	return buf;
}

std::string to_lower(const std::string& s) {
	std::string r;
	r.reserve(s.size());
	for (char c : s)
		r += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

std::string truncate(const std::string& s, size_t n) {
	return s.size() <= n ? s : s.substr(0, n) + "…";
}

}  // namespace

SessionBrowser::SessionBrowser(BRect frame, BWindow* parent)
	: BWindow(frame, "Pluto — Sessions", B_TITLED_WINDOW,
	          B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	  fParent(parent) {

	fFilter = new BTextControl("filter", "Search:", "",
		new BMessage(kMsgFilter));
	fFilter->SetModificationMessage(new BMessage(kMsgFilter));

	fList = new BListView("sessionList");
	fList->SetSelectionMessage(new BMessage(kMsgSelect));
	fList->SetInvocationMessage(new BMessage(kMsgDblClick));
	auto* scroll = new BScrollView("scroll", fList,
		0, false, true, B_FANCY_BORDER);

	fDetail = new BStringView("detail", "Select a session.");
	fDetail->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	fLoadBtn   = new BButton("load", "Load", new BMessage(kMsgLoad));
	fDeleteBtn = new BButton("delete", "Delete", new BMessage(kMsgDelete));
	fLoadBtn->SetEnabled(false);
	fDeleteBtn->SetEnabled(false);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fFilter)
		.Add(scroll, 4.0f)
		.Add(fDetail)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fDeleteBtn)
			.Add(fLoadBtn)
		.End();

	SetSizeLimits(420, 1400, 320, 900);
	_RefreshList();
	fFilter->MakeFocus(true);
}

void SessionBrowser::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgFilter:
			_RefreshList(fFilter->Text());
			break;
		case kMsgSelect:
			_SelectionChanged();
			break;
		case kMsgDblClick:
			_LoadSelected();
			break;
		case kMsgLoad:
			_LoadSelected();
			break;
		case kMsgDelete:
			_DeleteSelected();
			break;
		default:
			BWindow::MessageReceived(msg);
	}
}

void SessionBrowser::_RefreshList(const char* filter) {
	while (fList->CountItems() > 0)
		delete fList->RemoveItem((int32)0);
	fPaths.clear();

	std::string q = filter ? to_lower(filter) : "";
	auto all = SessionStore::list();

	for (const auto& m : all) {
		std::string title = m.title.empty() ? m.filename : m.title;
		// Filter by title/model/tags.
		if (!q.empty()) {
			std::string haystack = to_lower(title) + " "
				+ to_lower(m.model) + " " + to_lower(m.tags);
			if (haystack.find(q) == std::string::npos)
				continue;
		}

		std::string label = format_date(m.updated) + "  "
			+ truncate(title, 40);
		if (!m.model.empty()) {
			std::string short_model = m.model;
			if (short_model.rfind("claude-", 0) == 0)
				short_model = short_model.substr(7);
			label += "  (" + truncate(short_model, 15) + ")";
		}
		label += "  " + std::to_string(m.tokens_in + m.tokens_out) + " tok";

		fList->AddItem(new BStringItem(label.c_str()));
		fPaths.push_back(m.path);
	}

	if (fPaths.empty()) {
		fDetail->SetText(q.empty()
			? "No saved sessions."
			: "No sessions matching the filter.");
	}

	fLoadBtn->SetEnabled(false);
	fDeleteBtn->SetEnabled(false);
}

void SessionBrowser::_SelectionChanged() {
	int32 sel = fList->CurrentSelection();
	bool has = (sel >= 0 && (size_t)sel < fPaths.size());
	fLoadBtn->SetEnabled(has);
	fDeleteBtn->SetEnabled(has);
	if (!has) return;

	// Load metadata for detail display.
	std::vector<Message> tmp;
	SessionMetadata meta;
	std::string error;
	if (!SessionStore::load(fPaths[sel], tmp, meta, error)) return;

	std::string info = meta.title.empty() ? meta.filename : meta.title;
	info += "\n" + format_date(meta.created) + " — "
		+ format_date(meta.updated);
	info += "\nModel: " + meta.model;
	info += "  |  " + std::to_string(meta.turns) + " turns";
	info += "  |  in " + std::to_string(meta.tokens_in)
		+ " / out " + std::to_string(meta.tokens_out) + " tok";
	if (!meta.tags.empty()) info += "\nTags: " + meta.tags;
	fDetail->SetText(info.c_str());
}

void SessionBrowser::_LoadSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fPaths.size()) return;

	if (fParent) {
		BMessage m(kMsgSessionPicked);
		m.AddString("path", fPaths[sel].c_str());
		BMessenger(fParent).SendMessage(&m);
	}
	Quit();
}

void SessionBrowser::_DeleteSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fPaths.size()) return;

	BAlert* alert = new BAlert("Confirm",
		"Delete this session?", "Cancel", "Delete");
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1) return;

	std::string error;
	SessionStore::remove(fPaths[sel], error);
	_RefreshList(fFilter->Text());
}

}  // namespace gui
}  // namespace pluto
