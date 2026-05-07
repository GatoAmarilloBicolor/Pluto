#include "ScheduleDialog.h"

#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>

#include <cstdlib>
#include <string>

#include "../schedule.h"

namespace pluto {
namespace gui {

namespace {

const uint32 kMsgAdd    = 'scAd';
const uint32 kMsgRemove = 'scRm';
const uint32 kMsgToggle = 'scTg';
const uint32 kMsgRun    = 'scRn';
const uint32 kMsgSelect = 'scSl';

// AddScheduleDialog internals
const uint32 kMsgOk     = 'scOk';
const uint32 kMsgCancel = 'scCa';
const uint32 kMsgBrowse = 'scBr';
const uint32 kMsgFileSelected = 'scFs';

bool _IsDark() {
	rgb_color bg = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	return (bg.red + bg.green + bg.blue) < 384;
}

}  // namespace

// ---------------------------------------------------------------------------
// ScheduleListItem — custom two-line list item
// ---------------------------------------------------------------------------

ScheduleListItem::ScheduleListItem(const std::string& name,
                                   const std::string& info, bool enabled)
	: BListItem(), fName(name), fInfo(info), fEnabled(enabled) {
}

void ScheduleListItem::DrawItem(BView* owner, BRect frame, bool /*complete*/) {
	bool dark = _IsDark();
	rgb_color bg = IsSelected()
		? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR)
		: ui_color(B_LIST_BACKGROUND_COLOR);

	owner->SetLowColor(bg);
	owner->SetHighColor(bg);
	owner->FillRect(frame);

	// Line 1: name (bold) + status badge
	BFont bold = *be_plain_font;
	bold.SetFace(B_BOLD_FACE);
	owner->SetFont(&bold);

	rgb_color textColor = IsSelected()
		? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
		: ui_color(B_LIST_ITEM_TEXT_COLOR);
	owner->SetHighColor(textColor);

	float nameY = frame.top + bold.Size() + 4.0f;
	owner->DrawString(fName.c_str(), BPoint(frame.left + 8, nameY));

	// Status on the right
	BFont small = *be_plain_font;
	small.SetSize(small.Size() - 1);
	owner->SetFont(&small);

	const char* status = fEnabled ? "active" : "paused";
	rgb_color statusCol = fEnabled
		? (dark ? (rgb_color){ 80, 200, 120, 255 } : (rgb_color){ 30, 130, 60, 255 })
		: (dark ? (rgb_color){ 200, 160, 80, 255 } : (rgb_color){ 160, 120, 40, 255 });
	owner->SetHighColor(statusCol);
	float sw = small.StringWidth(status);
	owner->DrawString(status, BPoint(frame.right - sw - 8, nameY));

	// Line 2: info (dim)
	if (!fInfo.empty()) {
		rgb_color dimColor = dark
			? (rgb_color){ 170, 170, 170, 255 }
			: (rgb_color){ 100, 100, 100, 255 };
		owner->SetHighColor(dimColor);

		std::string desc = fInfo;
		float maxW = frame.Width() - 16;
		while (desc.size() > 10 && small.StringWidth(desc.c_str()) > maxW) {
			desc.resize(desc.size() - 4);
			desc += "...";
		}
		float descY = nameY + small.Size() + 3.0f;
		owner->DrawString(desc.c_str(), BPoint(frame.left + 8, descY));
	}
}

void ScheduleListItem::Update(BView* owner, const BFont* font) {
	BListItem::Update(owner, font);
	float lineH = font->Size() + 4.0f;
	SetHeight(lineH * 2 + 8.0f);
}

// ==================================================================
// ScheduleDialog — main list
// ==================================================================

ScheduleDialog::ScheduleDialog(BRect frame)
	: BWindow(frame, "Pluto — Schedules", B_TITLED_WINDOW,
	          B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE) {

	// --- Left panel: schedule list ---
	fList = new BListView("scheduleList");
	fList->SetSelectionMessage(new BMessage(kMsgSelect));
	auto* listScroll = new BScrollView("listScroll", fList,
		0, false, true, B_FANCY_BORDER);

	fAddBtn    = new BButton("add",    "Add…",           new BMessage(kMsgAdd));
	fRemoveBtn = new BButton("remove", "Remove",         new BMessage(kMsgRemove));
	fToggleBtn = new BButton("toggle", "Enable/Disable", new BMessage(kMsgToggle));
	fRunBtn    = new BButton("run",    "Run now",         new BMessage(kMsgRun));
	fRemoveBtn->SetEnabled(false);
	fToggleBtn->SetEnabled(false);
	fRunBtn->SetEnabled(false);

	// --- Right panel: detail ---
	fDetailView = new BTextView("detailView");
	fDetailView->MakeEditable(false);
	fDetailView->MakeSelectable(true);
	fDetailView->SetStylable(true);
	fDetailView->SetWordWrap(true);
	auto* detailScroll = new BScrollView("detailScroll", fDetailView,
		0, false, true, B_FANCY_BORDER);

	BView* rightPanel = new BView("rightPanel", 0);
	rightPanel->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(rightPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(new BStringView("detailLabel", "Details"))
		.Add(detailScroll, 1.0f);

	// Left panel: list + buttons
	BView* leftPanel = new BView("leftPanel", 0);
	BLayoutBuilder::Group<>(leftPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(listScroll, 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fAddBtn)
			.Add(fRemoveBtn)
			.Add(fToggleBtn)
			.Add(fRunBtn)
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
}

void ScheduleDialog::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgAdd:
			_AddSchedule();
			break;
		case kMsgRemove:
			_RemoveSelected();
			break;
		case kMsgToggle:
			_ToggleSelected();
			break;
		case kMsgRun:
			_RunSelected();
			break;
		case kMsgScheduleCreated:
			_RefreshList();
			break;
		case kMsgSelect:
			_SelectionChanged();
			break;
		default:
			BWindow::MessageReceived(msg);
	}
}

void ScheduleDialog::_RefreshList() {
	while (fList->CountItems() > 0)
		delete fList->RemoveItem((int32)0);
	fNames.clear();

	auto scheds = ListSchedules();
	if (scheds.empty()) {
		fDetailView->SetText("");

		bool dark = _IsDark();
		BFont boldFont = *be_plain_font;
		boldFont.SetFace(B_BOLD_FACE);
		rgb_color textCol = ui_color(B_DOCUMENT_TEXT_COLOR);
		rgb_color dimCol = dark
			? (rgb_color){ 160, 160, 160, 255 }
			: (rgb_color){ 110, 110, 110, 255 };

		text_run_array run1;
		run1.count = 1;
		run1.runs[0] = { 0, boldFont, textCol };
		fDetailView->Insert("No schedules\n\n", &run1);

		BFont plain = *be_plain_font;
		text_run_array run2;
		run2.count = 1;
		run2.runs[0] = { 0, plain, dimCol };
		fDetailView->Insert(
			"Click \"Add…\" to create a new schedule.",
			&run2);
		return;
	}
	for (const auto& s : scheds) {
		std::string info;
		if (!s.event.empty()) info += "Event: " + s.event;
		if (s.interval_mins > 0) {
			if (!info.empty()) info += "  ·  ";
			info += "Every " + std::to_string(s.interval_mins) + " min";
		}
		if (info.empty()) info = s.prompt_file;

		fList->AddItem(new ScheduleListItem(s.name, info, s.enabled));
		fNames.push_back(s.name);
	}
}

void ScheduleDialog::_SelectionChanged() {
	int32 sel = fList->CurrentSelection();
	bool has = (sel >= 0 && (size_t)sel < fNames.size());
	fRemoveBtn->SetEnabled(has);
	fToggleBtn->SetEnabled(has);
	fRunBtn->SetEnabled(has);

	if (!has) {
		fDetailView->SetText("");
		return;
	}

	std::string name = fNames[sel];
	Schedule sched;
	if (!LoadSchedule(name, sched)) return;

	bool dark = _IsDark();
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

	// Title
	BFont titleFont = *be_plain_font;
	titleFont.SetFace(B_BOLD_FACE);
	titleFont.SetSize(titleFont.Size() + 2);
	text_run_array titleRun;
	titleRun.count = 1;
	titleRun.runs[0] = { 0, titleFont, textCol };
	std::string titleStr = sched.name + "\n";
	fDetailView->Insert(titleStr.c_str(), &titleRun);

	// Status line
	std::string meta;
	meta += sched.enabled ? "active" : "paused";
	if (!sched.event.empty()) meta += "  ·  on " + sched.event;
	if (sched.interval_mins > 0)
		meta += "  ·  every " + std::to_string(sched.interval_mins) + " min";
	if (!sched.model.empty()) meta += "  ·  " + sched.model;
	meta += "\n";
	text_run_array metaRun;
	metaRun.count = 1;
	metaRun.runs[0] = { 0, smallFont, dimCol };
	fDetailView->Insert(meta.c_str(), &metaRun);

	// Prompt file
	{
		text_run_array labelRun;
		labelRun.count = 1;
		labelRun.runs[0] = { 0, boldFont, dimCol };
		fDetailView->Insert("\nPrompt file\n", &labelRun);

		text_run_array valRun;
		valRun.count = 1;
		valRun.runs[0] = { 0, plainFont, accentCol };
		std::string path = "  " + sched.prompt_file + "\n";
		fDetailView->Insert(path.c_str(), &valRun);
	}
}

void ScheduleDialog::_AddSchedule() {
	BRect frame(Frame());
	frame.InsetBy(40, 30);
	auto* dialog = new AddScheduleDialog(frame, this);
	dialog->CenterIn(Frame());
	dialog->Show();
}

void ScheduleDialog::_RemoveSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;
	std::string name = fNames[sel];

	BString confirm;
	confirm.SetToFormat("Remove schedule \"%s\"?", name.c_str());
	BAlert* alert = new BAlert("Confirm", confirm.String(), "Cancel", "Remove");
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1) return;

	std::string error;
	if (!RemoveSchedule(name, error)) {
		(new BAlert("Error", error.c_str(), "OK"))->Go(nullptr);
	}
	_RefreshList();
}

void ScheduleDialog::_ToggleSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;
	std::string name = fNames[sel];

	Schedule sched;
	if (!LoadSchedule(name, sched)) return;
	sched.enabled = !sched.enabled;
	std::string error;
	SaveSchedule(sched, error);
	InstallLaunchJob(sched, error);
	_RefreshList();
}

void ScheduleDialog::_RunSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;
	std::string name = fNames[sel];

	Schedule sched;
	if (!LoadSchedule(name, sched)) return;

	std::string cmd = "Terminal -t 'pluto schedule: " + name
		+ "' /bin/sh -c 'pluto agent run " + sched.prompt_file;
	if (!sched.model.empty()) cmd += " --model " + sched.model;
	cmd += "; echo; echo \"— Done. Press Enter —\"; read' &";
	std::system(cmd.c_str());
}

// ==================================================================
// AddScheduleDialog — creation form
// ==================================================================

AddScheduleDialog::AddScheduleDialog(BRect frame, BWindow* parent)
	: BWindow(frame, "New schedule", B_TITLED_WINDOW,
	          B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	  fParent(parent),
	  fFilePanel(nullptr) {

	fName       = new BTextControl("Name:", "", nullptr);
	fPromptFile = new BTextControl("Prompt file:", "", nullptr);
	fInterval   = new BTextControl("Interval (min):", "0", nullptr);
	fModel      = new BTextControl("Model (optional):", "", nullptr);

	fName->SetText("my-schedule");

	auto* browseBtn = new BButton("browse", "Browse…",
		new BMessage(kMsgBrowse));

	auto* eventMenu = new BPopUpMenu("(none)");
	eventMenu->AddItem(new BMenuItem("(none)", nullptr));
	eventMenu->AddItem(new BMenuItem("boot", nullptr));
	eventMenu->AddItem(new BMenuItem("login", nullptr));
	fEventField = new BMenuField("Event:", eventMenu);

	auto* okBtn = new BButton("ok", "Create", new BMessage(kMsgOk));
	auto* cancelBtn = new BButton("cancel", "Cancel", new BMessage(kMsgCancel));
	okBtn->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fName->CreateLabelLayoutItem(), 0, 0)
			.Add(fName->CreateTextViewLayoutItem(), 1, 0, 2)
			.Add(fPromptFile->CreateLabelLayoutItem(), 0, 1)
			.Add(fPromptFile->CreateTextViewLayoutItem(), 1, 1)
			.Add(browseBtn, 2, 1)
			.Add(fEventField->CreateLabelLayoutItem(), 0, 2)
			.Add(fEventField->CreateMenuBarLayoutItem(), 1, 2, 2)
			.Add(fInterval->CreateLabelLayoutItem(), 0, 3)
			.Add(fInterval->CreateTextViewLayoutItem(), 1, 3, 2)
			.Add(fModel->CreateLabelLayoutItem(), 0, 4)
			.Add(fModel->CreateTextViewLayoutItem(), 1, 4, 2)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(cancelBtn)
			.Add(okBtn)
		.End();

	SetSizeLimits(400, 900, 220, 450);
	fName->MakeFocus(true);
}

AddScheduleDialog::~AddScheduleDialog() {
	delete fFilePanel;
}

void AddScheduleDialog::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgBrowse: {
			if (!fFilePanel) {
				BMessage fileMsg(kMsgFileSelected);
				fFilePanel = new BFilePanel(B_OPEN_PANEL,
					new BMessenger(this), nullptr,
					B_FILE_NODE, false, &fileMsg,
					nullptr, true, true);
				fFilePanel->Window()->SetTitle("Select prompt file");
			}
			fFilePanel->Show();
			break;
		}

		case kMsgFileSelected: {
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				if (path.InitCheck() == B_OK)
					fPromptFile->SetText(path.Path());
			}
			break;
		}

		case B_REFS_RECEIVED: {
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				if (path.InitCheck() == B_OK)
					fPromptFile->SetText(path.Path());
			}
			break;
		}

		case kMsgOk: {
			std::string name = fName->Text() ? fName->Text() : "";
			std::string file = fPromptFile->Text() ? fPromptFile->Text() : "";

			if (name.empty()) {
				(new BAlert("Error", "Name is required.", "OK"))->Go(nullptr);
				return;
			}
			if (file.empty()) {
				(new BAlert("Error", "Prompt file is required.", "OK"))->Go(nullptr);
				return;
			}

			Schedule sched;
			sched.name = name;
			sched.prompt_file = file;

			BMenuItem* sel = fEventField->Menu()->FindMarked();
			if (sel) {
				std::string ev = sel->Label();
				if (ev != "(none)") sched.event = ev;
			}

			std::string interval_str = fInterval->Text() ? fInterval->Text() : "0";
			sched.interval_mins = std::atoi(interval_str.c_str());

			std::string model = fModel->Text() ? fModel->Text() : "";
			if (!model.empty()) sched.model = model;

			sched.enabled = true;

			std::string error;
			if (!SaveSchedule(sched, error)) {
				(new BAlert("Error", error.c_str(), "OK"))->Go(nullptr);
				return;
			}
			InstallLaunchJob(sched, error);

			if (fParent) {
				BMessage notify(kMsgScheduleCreated);
				BMessenger(fParent).SendMessage(&notify);
			}

			BString info;
			info.SetToFormat("Schedule \"%s\" created.", name.c_str());
			(new BAlert("Done", info.String(), "OK"))->Go(nullptr);

			Quit();
			break;
		}
		case kMsgCancel:
			Quit();
			break;
		default:
			BWindow::MessageReceived(msg);
	}
}

}  // namespace gui
}  // namespace pluto
