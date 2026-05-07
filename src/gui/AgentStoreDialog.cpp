#include "AgentStoreDialog.h"

#include <Alert.h>
#include <Box.h>
#include <Button.h>
#include <FilePanel.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Messenger.h>
#include <Path.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <SplitView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "../agent_store.h"

namespace pluto {
namespace gui {

namespace {

const uint32 kMsgSelect       = 'asSl';
const uint32 kMsgInstall      = 'asIn';
const uint32 kMsgRemove       = 'asRm';
const uint32 kMsgUpdate       = 'asUp';
const uint32 kMsgSearch       = 'asSr';
const uint32 kMsgFileSelected = 'asFs';

// Returns true if Haiku is using a dark theme.
bool _IsDark() {
	rgb_color bg = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	return (bg.red + bg.green + bg.blue) < 384;
}

}  // namespace

// ---------------------------------------------------------------------------
// AgentListItem — custom two-line list item
// ---------------------------------------------------------------------------

AgentListItem::AgentListItem(const std::string& name,
                             const std::string& version,
                             const std::string& description,
                             const std::string& author)
	: BListItem(),
	  fName(name),
	  fVersion(version),
	  fDescription(description),
	  fAuthor(author) {
}

void AgentListItem::DrawItem(BView* owner, BRect frame, bool /*complete*/) {
	bool dark = _IsDark();
	rgb_color bg;
	if (IsSelected()) {
		bg = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
	} else {
		bg = ui_color(B_LIST_BACKGROUND_COLOR);
	}

	owner->SetLowColor(bg);
	owner->SetHighColor(bg);
	owner->FillRect(frame);

	// Line 1: name (bold) + version badge
	BFont bold = *be_plain_font;
	bold.SetFace(B_BOLD_FACE);
	owner->SetFont(&bold);

	rgb_color textColor = IsSelected()
		? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
		: ui_color(B_LIST_ITEM_TEXT_COLOR);
	owner->SetHighColor(textColor);

	float nameY = frame.top + bold.Size() + 4.0f;
	owner->DrawString(fName.c_str(), BPoint(frame.left + 8, nameY));

	// Version badge (small, dimmed, after name)
	if (!fVersion.empty()) {
		BFont small = *be_plain_font;
		small.SetSize(small.Size() - 1);
		owner->SetFont(&small);

		float nameW = bold.StringWidth(fName.c_str());
		rgb_color dimColor = dark
			? (rgb_color){ 160, 160, 160, 255 }
			: (rgb_color){ 120, 120, 120, 255 };
		owner->SetHighColor(dimColor);

		std::string vstr = "v" + fVersion;
		owner->DrawString(vstr.c_str(),
			BPoint(frame.left + 8 + nameW + 8, nameY));
	}

	// Author on the right side of line 1
	if (!fAuthor.empty()) {
		BFont small = *be_plain_font;
		small.SetSize(small.Size() - 1);
		owner->SetFont(&small);

		rgb_color authorColor = dark
			? (rgb_color){ 130, 170, 210, 255 }
			: (rgb_color){  70, 110, 160, 255 };
		owner->SetHighColor(authorColor);

		float aw = small.StringWidth(fAuthor.c_str());
		owner->DrawString(fAuthor.c_str(),
			BPoint(frame.right - aw - 8, nameY));
	}

	// Line 2: description (plain, dimmed)
	if (!fDescription.empty()) {
		BFont plain = *be_plain_font;
		plain.SetSize(plain.Size() - 1);
		owner->SetFont(&plain);

		rgb_color descColor = dark
			? (rgb_color){ 170, 170, 170, 255 }
			: (rgb_color){ 100, 100, 100, 255 };
		owner->SetHighColor(descColor);

		// Truncate to fit
		std::string desc = fDescription;
		float maxW = frame.Width() - 16;
		while (desc.size() > 10 && plain.StringWidth(desc.c_str()) > maxW) {
			desc.resize(desc.size() - 4);
			desc += "...";
		}
		float descY = nameY + plain.Size() + 3.0f;
		owner->DrawString(desc.c_str(), BPoint(frame.left + 8, descY));
	}
}

void AgentListItem::Update(BView* owner, const BFont* font) {
	BListItem::Update(owner, font);
	// Two lines: name + description
	float lineH = font->Size() + 4.0f;
	float height = lineH * 2 + 8.0f;
	SetHeight(height);
}

// ---------------------------------------------------------------------------
// AgentStoreDialog
// ---------------------------------------------------------------------------

AgentStoreDialog::AgentStoreDialog(BRect frame)
	: BWindow(frame, "Pluto — Agents", B_TITLED_WINDOW,
	          B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	  fFilePanel(nullptr) {

	// --- Left panel: search + agent list ---
	fSearchField = new BTextControl("search", "Search:", "",
		new BMessage(kMsgSearch));
	fSearchField->SetModificationMessage(new BMessage(kMsgSearch));

	fList = new BListView("agentList");
	fList->SetSelectionMessage(new BMessage(kMsgSelect));
	auto* listScroll = new BScrollView("listScroll", fList,
		0, false, true, B_FANCY_BORDER);

	// Buttons below list
	fInstallBtn = new BButton("install", "Install...",
		new BMessage(kMsgInstall));
	fRemoveBtn  = new BButton("remove",  "Remove",
		new BMessage(kMsgRemove));
	fRemoveBtn->SetEnabled(false);
	fUpdateBtn  = new BButton("update",  "Update",
		new BMessage(kMsgUpdate));
	fUpdateBtn->SetEnabled(false);

	// --- Right panel: detail + prompt ---

	// Detail view (styled multi-line text)
	fDetailView = new BTextView("detailView");
	fDetailView->MakeEditable(false);
	fDetailView->MakeSelectable(true);
	fDetailView->SetStylable(true);
	fDetailView->SetWordWrap(true);
	auto* detailScroll = new BScrollView("detailScroll", fDetailView,
		0, false, true, B_FANCY_BORDER);

	// Prompt body view (monospace)
	fPromptView = new BTextView("promptView");
	fPromptView->MakeEditable(false);
	fPromptView->MakeSelectable(true);
	fPromptView->SetStylable(false);
	fPromptView->SetWordWrap(true);
	BFont mono = *be_fixed_font;
	mono.SetSize(mono.Size() - 1);
	fPromptView->SetFontAndColor(&mono);
	auto* promptScroll = new BScrollView("promptScroll", fPromptView,
		0, false, true, B_FANCY_BORDER);

	// Right panel: detail on top, prompt below
	BView* rightPanel = new BView("rightPanel", 0);
	rightPanel->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(rightPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(new BStringView("detailLabel", "Details"))
		.Add(detailScroll, 1.0f)
		.Add(new BStringView("promptLabel", "Agent prompt"))
		.Add(promptScroll, 2.0f);

	// Left panel: list + buttons
	BView* leftPanel = new BView("leftPanel", 0);
	BLayoutBuilder::Group<>(leftPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(fSearchField)
		.Add(listScroll, 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fInstallBtn)
			.Add(fRemoveBtn)
			.Add(fUpdateBtn)
			.AddGlue()
		.End();

	// Main split: left list | right detail
	fSplitView = new BSplitView(B_HORIZONTAL);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddSplit(fSplitView)
			.Add(leftPanel, 2.0f)
			.Add(rightPanel, 3.0f)
		.End();

	SetSizeLimits(700, 1600, 400, 950);
	_RefreshList();
	_UpdateTitle();
}

AgentStoreDialog::~AgentStoreDialog() {
	delete fFilePanel;
}

void AgentStoreDialog::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgSelect:
			_SelectionChanged();
			break;
		case kMsgInstall:
			_InstallFromPath();
			break;
		case kMsgRemove:
			_RemoveSelected();
			break;
		case kMsgUpdate:
			_UpdateSelected();
			break;
		case kMsgSearch: {
			std::string q = fSearchField->Text();
			if (q != fSearchQuery) {
				fSearchQuery = q;
				_RefreshList();
			}
			break;
		}
		case kMsgFileSelected: {
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				_InstallWithReview(path.Path());
			}
			break;
		}
		case B_REFS_RECEIVED: {
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				_InstallWithReview(path.Path());
			}
			break;
		}
		default:
			BWindow::MessageReceived(msg);
	}
}

void AgentStoreDialog::_RefreshList() {
	while (fList->CountItems() > 0)
		delete fList->RemoveItem((int32)0);
	fNames.clear();

	std::vector<Agent> agents;
	if (fSearchQuery.empty())
		agents = ListAgents();
	else
		agents = SearchAgents(fSearchQuery);
	if (agents.empty()) {
		fDetailView->SetText("");
		fPromptView->SetText("");

		// Show a helpful empty state.
		BFont bold = *be_plain_font;
		bold.SetFace(B_BOLD_FACE);

		rgb_color textCol = ui_color(B_DOCUMENT_TEXT_COLOR);
		rgb_color dimCol = _IsDark()
			? (rgb_color){ 160, 160, 160, 255 }
			: (rgb_color){ 110, 110, 110, 255 };

		fDetailView->SetText("");
		fDetailView->Insert("No agents installed\n\n",
			strlen("No agents installed\n\n"), nullptr);

		// Re-insert with styling
		fDetailView->SetText("");
		text_run_array run1;
		run1.count = 1;
		run1.runs[0] = { 0, bold, textCol };
		fDetailView->Insert("No agents installed\n\n", &run1);

		BFont plain = *be_plain_font;
		text_run_array run2;
		run2.count = 1;
		run2.runs[0] = { 0, plain, dimCol };
		fDetailView->Insert(
			"Install agents from local directories or Git repositories.\n\n"
			"Example agents are included in:\n"
			"  examples/agents/haiku-dev\n"
			"  examples/agents/code-reviewer\n"
			"  examples/agents/shell-assistant\n"
			"  examples/agents/project-onboarder\n\n"
			"Click 'Install...' to browse for an agent directory,\n"
			"or drag a folder from Tracker onto this window.",
			&run2);
		return;
	}
	for (const auto& a : agents) {
		fList->AddItem(new AgentListItem(
			a.name, a.version, a.description, a.author));
		fNames.push_back(a.name);
	}
}

void AgentStoreDialog::_SelectionChanged() {
	int32 sel = fList->CurrentSelection();
	bool has = (sel >= 0 && (size_t)sel < fNames.size());
	fRemoveBtn->SetEnabled(has);
	// Update button: only enabled if agent has a source URL.
	if (has) {
		std::string url = AgentSourceUrl(fNames[sel]);
		fUpdateBtn->SetEnabled(!url.empty());
	} else {
		fUpdateBtn->SetEnabled(false);
	}

	if (!has) {
		fDetailView->SetText("");
		fPromptView->SetText("");
		return;
	}

	std::string name = fNames[sel];
	std::string agent_md = AgentsDir() + "/" + name + "/AGENT.md";
	Agent a;
	if (!ParseAgent(agent_md, a)) return;

	bool dark = _IsDark();

	// --- Build styled detail text ---
	fDetailView->SetText("");

	BFont boldFont = *be_plain_font;
	boldFont.SetFace(B_BOLD_FACE);
	BFont plainFont = *be_plain_font;
	BFont smallFont = *be_plain_font;
	smallFont.SetSize(smallFont.Size() - 1);

	rgb_color textCol = ui_color(B_DOCUMENT_TEXT_COLOR);
	rgb_color dimCol = dark
		? (rgb_color){ 160, 160, 160, 255 }
		: (rgb_color){ 110, 110, 110, 255 };
	rgb_color accentCol = dark
		? (rgb_color){ 100, 180, 240, 255 }
		: (rgb_color){  40,  95, 180, 255 };
	rgb_color greenCol = dark
		? (rgb_color){  80, 200, 120, 255 }
		: (rgb_color){  30, 130,  60, 255 };

	// Agent name (large bold)
	BFont titleFont = *be_plain_font;
	titleFont.SetFace(B_BOLD_FACE);
	titleFont.SetSize(titleFont.Size() + 2);
	text_run_array titleRun;
	titleRun.count = 1;
	titleRun.runs[0] = { 0, titleFont, textCol };
	std::string titleStr = a.name + "\n";
	fDetailView->Insert(titleStr.c_str(), &titleRun);

	// Version + author line
	if (!a.version.empty() || !a.author.empty()) {
		std::string meta;
		if (!a.version.empty()) meta += "v" + a.version;
		if (!a.author.empty()) {
			if (!meta.empty()) meta += "  ·  ";
			meta += "by " + a.author;
		}
		meta += "\n";
		text_run_array metaRun;
		metaRun.count = 1;
		metaRun.runs[0] = { 0, smallFont, dimCol };
		fDetailView->Insert(meta.c_str(), &metaRun);
	}

	// Description
	if (!a.description.empty()) {
		text_run_array descRun;
		descRun.count = 1;
		descRun.runs[0] = { 0, plainFont, textCol };
		std::string desc = "\n" + a.description + "\n";
		fDetailView->Insert(desc.c_str(), &descRun);
	}

	// Sections helper lambda
	auto addSection = [&](const char* label, const std::vector<std::string>& items,
	                       rgb_color tagColor) {
		if (items.empty()) return;
		text_run_array labelRun;
		labelRun.count = 1;
		labelRun.runs[0] = { 0, boldFont, dimCol };
		std::string sec = std::string("\n") + label + "\n";
		fDetailView->Insert(sec.c_str(), &labelRun);

		for (const auto& item : items) {
			text_run_array bulletRun;
			bulletRun.count = 1;
			bulletRun.runs[0] = { 0, plainFont, tagColor };
			std::string line = "  · " + item + "\n";
			fDetailView->Insert(line.c_str(), &bulletRun);
		}
	};

	addSection("Required tools", a.tools, accentCol);
	addSection("Skills", a.skills, greenCol);
	addSection("Routines", a.routines, greenCol);
	addSection("Commands", a.commands, greenCol);

	// --- Load prompt body ---
	std::ifstream f(agent_md);
	if (f) {
		std::ostringstream os;
		os << f.rdbuf();
		std::string content = os.str();

		// Skip frontmatter.
		if (content.size() >= 4 && content.substr(0, 4) == "---\n") {
			size_t end = content.find("\n---\n", 4);
			if (end != std::string::npos)
				content = content.substr(end + 5);
		}
		// Trim leading whitespace
		size_t first = content.find_first_not_of("\n\r ");
		if (first != std::string::npos)
			content = content.substr(first);
		fPromptView->SetText(content.c_str());
	} else {
		fPromptView->SetText("");
	}
}

void AgentStoreDialog::_InstallWithReview(const char* source) {
	std::string source_str = source;

	// Preview the agent before installing.
	Agent preview_a;
	std::string body;
	if (PreviewAgent(source_str, preview_a, body)) {
		// Build review text.
		BString review;
		review << "Name: " << preview_a.name.c_str() << "\n";
		if (!preview_a.version.empty())
			review << "Version: " << preview_a.version.c_str() << "\n";
		if (!preview_a.author.empty())
			review << "Author: " << preview_a.author.c_str() << "\n";
		if (!preview_a.tools.empty()) {
			review << "Tools: ";
			for (size_t i = 0; i < preview_a.tools.size(); ++i) {
				if (i > 0) review << ", ";
				review << preview_a.tools[i].c_str();
			}
			review << "\n";
		}
		review << "\n--- Prompt (first 500 chars) ---\n";
		std::string preview_body = body.substr(0, 500);
		if (body.size() > 500) preview_body += "\n...";
		review << preview_body.c_str();

		BAlert* alert = new BAlert("Review agent",
			review.String(), "Cancel", "Install",
			nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		int32 result = alert->Go();
		if (result != 1) return;
	}

	std::string error;
	if (InstallAgent(source_str, error)) {
		_RefreshList();
		_UpdateTitle();
		// Select the newly installed agent.
		for (int32 i = 0; i < fList->CountItems(); ++i) {
			auto* item = dynamic_cast<AgentListItem*>(fList->ItemAt(i));
			if (item && item->Name() == preview_a.name) {
				fList->Select(i);
				break;
			}
		}
		_SelectionChanged();
	} else {
		BAlert* err = new BAlert("Install failed",
			error.c_str(), "OK",
			nullptr, nullptr, B_WIDTH_AS_USUAL,
			B_WARNING_ALERT);
		err->Go();
	}
}

void AgentStoreDialog::_InstallFromPath() {
	if (!fFilePanel) {
		BMessage* msg = new BMessage(kMsgFileSelected);
		fFilePanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
			nullptr, B_DIRECTORY_NODE, false, msg);
		fFilePanel->SetButtonLabel(B_DEFAULT_BUTTON, "Install");
		fFilePanel->Window()->SetTitle("Select agent directory");
	}
	fFilePanel->Show();
}

void AgentStoreDialog::_RemoveSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;

	std::string name = fNames[sel];

	BString text;
	text.SetToFormat("Remove agent \"%s\"?\n\n"
		"This will also unregister any skills, routines,\n"
		"and commands installed with this agent.", name.c_str());

	BAlert* alert = new BAlert("Confirm removal",
		text.String(), "Cancel", "Remove",
		nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	int32 result = alert->Go();

	if (result == 1) {
		std::string error;
		if (RemoveAgent(name, error)) {
			_RefreshList();
			_UpdateTitle();
			fDetailView->SetText("");
			fPromptView->SetText("");
		} else {
			BAlert* err = new BAlert("Error",
				error.c_str(), "OK",
				nullptr, nullptr, B_WIDTH_AS_USUAL,
				B_WARNING_ALERT);
			err->Go();
		}
	}
}

void AgentStoreDialog::_UpdateSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;

	std::string name = fNames[sel];
	std::string url = AgentSourceUrl(name);
	if (url.empty()) return;

	// Disable button during update.
	fUpdateBtn->SetEnabled(false);
	fUpdateBtn->SetLabel("Updating...");

	std::string error;
	if (UpdateAgent(name, error)) {
		_RefreshList();
		_UpdateTitle();
		// Re-select the agent.
		for (int32 i = 0; i < fList->CountItems(); ++i) {
			auto* item = dynamic_cast<AgentListItem*>(fList->ItemAt(i));
			if (item && item->Name() == name) {
				fList->Select(i);
				break;
			}
		}
		_SelectionChanged();

		BAlert* alert = new BAlert("Updated",
			BString().SetToFormat("Agent '%s' updated successfully.",
				name.c_str()).String(),
			"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
		alert->Go();
	} else if (error.empty()) {
		BAlert* alert = new BAlert("Up to date",
			BString().SetToFormat("Agent '%s' is already up to date.",
				name.c_str()).String(),
			"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
		alert->Go();
	} else {
		BAlert* alert = new BAlert("Update failed",
			error.c_str(), "OK",
			nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
	}

	fUpdateBtn->SetLabel("Update");
	_SelectionChanged();  // re-enable if still valid
}

void AgentStoreDialog::_UpdateTitle() {
	int count = fList->CountItems();
	BString title;
	if (count == 0)
		title = "Pluto — Agents";
	else
		title.SetToFormat("pluto — Agents (%d)", count);
	SetTitle(title.String());
}

}  // namespace gui
}  // namespace pluto
