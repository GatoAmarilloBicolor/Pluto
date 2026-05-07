#include "RoutineDialog.h"

#include <Alert.h>
#include <Button.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Messenger.h>
#include <Notification.h>
#include <OS.h>
#include <Path.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#include <cstdlib>
#include <fstream>
#include <string>

#include "../agent/Loop.h"
#include "../provider/Anthropic.h"
#include "../workspace.h"

#include "../routine.h"

namespace pluto {
namespace gui {

namespace {

const uint32 kMsgSelect    = 'rtSl';
const uint32 kMsgRun       = 'rtRn';
const uint32 kMsgToggle    = 'rtTg';
const uint32 kMsgLog       = 'rtLg';
const uint32 kMsgImport    = 'rtIm';
const uint32 kMsgImportRef = 'rtIR';
const uint32 kMsgRunText   = 'rtTx';  // worker -> UI: append text
const uint32 kMsgRunDone   = 'rtDo';  // worker -> UI: run finished
const uint32 kMsgRunErr    = 'rtEr';  // worker -> UI: error

bool _IsDark() {
	rgb_color bg = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	return (bg.red + bg.green + bg.blue) < 384;
}

}  // namespace

// ---------------------------------------------------------------------------
// RoutineListItem — custom two-line list item
// ---------------------------------------------------------------------------

RoutineListItem::RoutineListItem(const std::string& name,
                                 const std::string& schedule, bool enabled)
	: BListItem(), fName(name), fSchedule(schedule), fEnabled(enabled) {
}

void RoutineListItem::DrawItem(BView* owner, BRect frame, bool /*complete*/) {
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

	// Line 2: schedule (dim)
	if (!fSchedule.empty()) {
		rgb_color dimColor = dark
			? (rgb_color){ 170, 170, 170, 255 }
			: (rgb_color){ 100, 100, 100, 255 };
		owner->SetHighColor(dimColor);

		std::string desc = fSchedule;
		float maxW = frame.Width() - 16;
		while (desc.size() > 10 && small.StringWidth(desc.c_str()) > maxW) {
			desc.resize(desc.size() - 4);
			desc += "...";
		}
		float descY = nameY + small.Size() + 3.0f;
		owner->DrawString(desc.c_str(), BPoint(frame.left + 8, descY));
	}
}

void RoutineListItem::Update(BView* owner, const BFont* font) {
	BListItem::Update(owner, font);
	float lineH = font->Size() + 4.0f;
	SetHeight(lineH * 2 + 8.0f);
}

// ---------------------------------------------------------------------------
// RoutineDialog
// ---------------------------------------------------------------------------

RoutineDialog::RoutineDialog(BRect frame,
                             pluto::Provider* provider,
                             pluto::Config* config,
                             pluto::ToolRegistry* tools)
	: BWindow(frame, "Pluto — Routines", B_TITLED_WINDOW,
	          B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	  fProvider(provider), fConfig(config), fTools(tools) {

	// --- Left panel: routine list ---
	fList = new BListView("routineList");
	fList->SetSelectionMessage(new BMessage(kMsgSelect));
	auto* listScroll = new BScrollView("listScroll", fList,
		0, false, true, B_FANCY_BORDER);

	fRunBtn    = new BButton("run",    "Run now",        new BMessage(kMsgRun));
	fToggleBtn = new BButton("toggle", "Enable/Disable", new BMessage(kMsgToggle));
	fLogBtn    = new BButton("log",    "Show log",       new BMessage(kMsgLog));
	fImportBtn = new BButton("import", "Import…",       new BMessage(kMsgImport));
	fRunBtn->SetEnabled(false);
	fToggleBtn->SetEnabled(false);
	fLogBtn->SetEnabled(false);

	// File panel for importing .md routine files.
	BMessage* panelMsg = new BMessage(kMsgImportRef);
	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), nullptr,
		B_FILE_NODE, false, panelMsg, nullptr, true);
	fOpenPanel->SetButtonLabel(B_DEFAULT_BUTTON, "Import");

	// --- Right panel: detail + log ---
	fDetailView = new BTextView("detailView");
	fDetailView->MakeEditable(false);
	fDetailView->MakeSelectable(true);
	fDetailView->SetStylable(true);
	fDetailView->SetWordWrap(true);
	auto* detailScroll = new BScrollView("detailScroll", fDetailView,
		0, false, true, B_FANCY_BORDER);

	fLogView = new BTextView("logView");
	fLogView->MakeEditable(false);
	fLogView->MakeSelectable(true);
	fLogView->SetStylable(false);
	fLogView->SetWordWrap(true);
	BFont mono = *be_fixed_font;
	mono.SetSize(mono.Size() - 1);
	fLogView->SetFontAndColor(&mono);
	auto* logScroll = new BScrollView("logScroll", fLogView,
		0, false, true, B_FANCY_BORDER);

	BView* rightPanel = new BView("rightPanel", 0);
	rightPanel->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(rightPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(new BStringView("detailLabel", "Details"))
		.Add(detailScroll, 1.0f)
		.Add(new BStringView("logLabel", "Execution log"))
		.Add(logScroll, 2.0f);

	// Left panel: list + buttons
	BView* leftPanel = new BView("leftPanel", 0);
	BLayoutBuilder::Group<>(leftPanel, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(listScroll, 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fRunBtn)
			.Add(fToggleBtn)
			.Add(fLogBtn)
			.AddGlue()
			.Add(fImportBtn)
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

RoutineDialog::~RoutineDialog() {
	if (fRunning && fWorker >= 0) {
		// Wait for the worker to finish before destroying.
		status_t dummy;
		wait_for_thread(fWorker, &dummy);
	}
	delete fOpenPanel;
}

void RoutineDialog::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgSelect:
			_SelectionChanged();
			break;
		case kMsgRun:
			_RunSelected();
			break;
		case kMsgToggle:
			_ToggleSelected();
			break;
		case kMsgLog:
			_ShowLog();
			break;
		case kMsgImport:
			_ImportRoutine();
			break;
		case kMsgImportRef:
			_HandleImportRef(msg);
			break;
		case kMsgRunText: {
			const char* text = nullptr;
			if (msg->FindString("text", &text) == B_OK && text) {
				fLogView->Insert(fLogView->TextLength(), text, strlen(text));
				fLogView->ScrollToOffset(fLogView->TextLength());
				fRunOutput += text;
			}
			break;
		}
		case kMsgRunErr: {
			const char* err = nullptr;
			if (msg->FindString("error", &err) == B_OK && err) {
				std::string line = std::string("[error] ") + err + "\n";
				fLogView->Insert(fLogView->TextLength(),
					line.c_str(), line.size());
				fLogView->ScrollToOffset(fLogView->TextLength());
				fRunOutput += line;
			}
			break;
		}
		case kMsgRunDone: {
			const char* name = nullptr;
			msg->FindString("name", &name);
			if (name) {
				LogRoutineRun(name, fRunOutput);
				std::string footer = "\n— routine '" + std::string(name) + "' done —\n";
				fLogView->Insert(fLogView->TextLength(),
					footer.c_str(), footer.size());
				fLogView->ScrollToOffset(fLogView->TextLength());
			}
			fRunOutput.clear();
			fRunning = false;
			fRunBtn->SetEnabled(fList->CurrentSelection() >= 0);
			fRunBtn->SetLabel("Run now");
			break;
		}
		default:
			BWindow::MessageReceived(msg);
	}
}

void RoutineDialog::_RefreshList() {
	while (fList->CountItems() > 0)
		delete fList->RemoveItem((int32)0);
	fNames.clear();

	auto routines = ListRoutines();
	if (routines.empty()) {
		fDetailView->SetText("");
		fLogView->SetText("");

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
		fDetailView->Insert("No routines found\n\n", &run1);

		BFont plain = *be_plain_font;
		text_run_array run2;
		run2.count = 1;
		run2.runs[0] = { 0, plain, dimCol };
		fDetailView->Insert(
			"Place .md files in ~/config/settings/pluto/routines/\n"
			"or copy from examples/routines/.",
			&run2);
		return;
	}
	for (const auto& r : routines) {
		fList->AddItem(new RoutineListItem(r.name, r.schedule, r.enabled));
		fNames.push_back(r.name);
	}
}

void RoutineDialog::_SelectionChanged() {
	int32 sel = fList->CurrentSelection();
	bool has = (sel >= 0 && (size_t)sel < fNames.size());
	fRunBtn->SetEnabled(has);
	fToggleBtn->SetEnabled(has);
	fLogBtn->SetEnabled(has);

	if (!has) {
		fDetailView->SetText("");
		fLogView->SetText("");
		return;
	}

	std::string name = fNames[sel];
	auto routines = ListRoutines();
	for (const auto& r : routines) {
		if (r.name != name) continue;

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
		rgb_color greenCol = dark
			? (rgb_color){  80, 200, 120, 255 }
			: (rgb_color){  30, 130,  60, 255 };

		// Title
		BFont titleFont = *be_plain_font;
		titleFont.SetFace(B_BOLD_FACE);
		titleFont.SetSize(titleFont.Size() + 2);
		text_run_array titleRun;
		titleRun.count = 1;
		titleRun.runs[0] = { 0, titleFont, textCol };
		std::string titleStr = r.name + "\n";
		fDetailView->Insert(titleStr.c_str(), &titleRun);

		// Status + schedule
		std::string meta;
		meta += r.enabled ? "active" : "paused";
		meta += "  ·  " + r.schedule;
		if (!r.model.empty()) meta += "  ·  " + r.model;
		meta += "\n";
		text_run_array metaRun;
		metaRun.count = 1;
		metaRun.runs[0] = { 0, smallFont, dimCol };
		fDetailView->Insert(meta.c_str(), &metaRun);

		// Options
		if (r.notify) {
			text_run_array optRun;
			optRun.count = 1;
			optRun.runs[0] = { 0, plainFont, greenCol };
			fDetailView->Insert("\nNotify on completion: yes\n", &optRun);
		}

		// Tools
		if (!r.tools.empty()) {
			text_run_array labelRun;
			labelRun.count = 1;
			labelRun.runs[0] = { 0, boldFont, dimCol };
			fDetailView->Insert("\nAllowed tools\n", &labelRun);

			for (const auto& t : r.tools) {
				text_run_array bulletRun;
				bulletRun.count = 1;
				bulletRun.runs[0] = { 0, plainFont, accentCol };
				std::string line = "  · " + t + "\n";
				fDetailView->Insert(line.c_str(), &bulletRun);
			}
		}

		// Prompt body
		if (!r.body.empty()) {
			text_run_array labelRun;
			labelRun.count = 1;
			labelRun.runs[0] = { 0, boldFont, dimCol };
			fDetailView->Insert("\nPrompt template\n", &labelRun);

			text_run_array bodyRun;
			bodyRun.count = 1;
			bodyRun.runs[0] = { 0, plainFont, textCol };
			std::string body = r.body;
			// Trim leading whitespace
			size_t first = body.find_first_not_of("\n\r ");
			if (first != std::string::npos) body = body.substr(first);
			body += "\n";
			fDetailView->Insert(body.c_str(), &bodyRun);
		}
		break;
	}
}

// Context passed to the routine worker thread.
struct RoutineWorkerCtx {
	BMessenger          target;
	pluto::Provider*    provider;
	pluto::Config*      config;
	pluto::ToolRegistry* tools;
	std::string         prompt;
	std::string         name;
	std::string         model;
	bool                notify;
};

static int32 _RoutineWorkerEntry(void* data) {
	auto* ctx = static_cast<RoutineWorkerCtx*>(data);

	// Build a fresh message list with just the routine prompt.
	std::vector<pluto::Message> messages;
	{
		pluto::Message user;
		user.role = "user";
		pluto::ContentBlock tb;
		tb.type = pluto::ContentBlock::TEXT;
		tb.text = ctx->prompt;
		user.content.push_back(std::move(tb));
		messages.push_back(std::move(user));
	}

	// Create a dedicated provider for the routine. Prefer API key over
	// OAuth — OAuth (Claude Max/Pro) has stricter rate limits and routines
	// can easily exhaust them. Fall back to the shared provider if no
	// API key is available.
	std::unique_ptr<pluto::Provider> own_provider;
	pluto::Provider* provider = ctx->provider;
	{
		std::string api_key = pluto::ResolveApiKey(*ctx->config);
		if (!api_key.empty()) {
			own_provider = std::make_unique<pluto::AnthropicProvider>(
				api_key, pluto::AuthType::API_KEY);
			provider = own_provider.get();
		}
	}

	// Ensure we run in the workspace directory so bash/tools work correctly.
	std::string ws = pluto::WorkspaceRoot();
	if (!ws.empty())
		chdir(ws.c_str());

	// Temporarily apply model override if the routine specifies one.
	std::string orig_model;
	if (!ctx->model.empty()) {
		orig_model = ctx->config->model;
		ctx->config->model = ctx->model;
	}

	pluto::UiCallbacks ui;
	ui.on_text = [&ctx](const std::string& text) {
		BMessage m(kMsgRunText);
		m.AddString("text", text.c_str());
		ctx->target.SendMessage(&m);
	};
	ui.on_tool_start = [&ctx](const std::string& tool_name,
	                           const std::string& /*preview*/) {
		std::string line = "\n[tool] " + tool_name + "\n";
		BMessage m(kMsgRunText);
		m.AddString("text", line.c_str());
		ctx->target.SendMessage(&m);
	};
	ui.on_tool_result = [&ctx](const std::string& /*tool_name*/,
	                            const std::string& /*input_json*/,
	                            const pluto::ToolResult& r) {
		std::string tag = r.is_error ? "[err] " : "[ok] ";
		std::string out = r.output;
		if (out.size() > 500) out = out.substr(0, 500) + "… [truncated]";
		std::string line = tag + out + "\n";
		BMessage m(kMsgRunText);
		m.AddString("text", line.c_str());
		ctx->target.SendMessage(&m);
	};
	ui.on_error = [&ctx](const std::string& err) {
		BMessage m(kMsgRunErr);
		m.AddString("error", err.c_str());
		ctx->target.SendMessage(&m);
	};
	ui.on_status = [&ctx](const std::string& status) {
		std::string line = "[status] " + status + "\n";
		BMessage m(kMsgRunText);
		m.AddString("text", line.c_str());
		ctx->target.SendMessage(&m);
	};

	pluto::RunTurn(*provider, *ctx->config, *ctx->tools,
	                messages, pluto::AutoAllow(), ui);

	// Restore model.
	if (!orig_model.empty())
		ctx->config->model = orig_model;

	if (ctx->notify) {
		BNotification n(B_INFORMATION_NOTIFICATION);
		n.SetTitle("Pluto routine");
		BString content;
		content.SetToFormat("Routine \"%s\" completed.", ctx->name.c_str());
		n.SetContent(content.String());
		n.Send();
	}

	BMessage done(kMsgRunDone);
	done.AddString("name", ctx->name.c_str());
	ctx->target.SendMessage(&done);

	delete ctx;
	return 0;
}

void RoutineDialog::_RunSelected() {
	if (fRunning) return;

	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;

	if (!fProvider || !fConfig || !fTools) {
		BAlert* alert = new BAlert("Run routine",
			"No provider available. Open a chat session first.",
			"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
		return;
	}

	std::string name = fNames[sel];
	auto routines = ListRoutines();
	for (const auto& r : routines) {
		if (r.name != name) continue;

		std::string prompt = ExpandTemplate(r.body);

		// Clear log view and show header.
		fLogView->SetText("");
		std::string header = "=== Running routine '" + name + "' ===\n\n";
		fLogView->Insert(header.c_str());

		fRunning = true;
		fRunOutput.clear();
		fRunBtn->SetLabel("Running…");
		fRunBtn->SetEnabled(false);

		auto* ctx = new RoutineWorkerCtx{
			BMessenger(this),
			fProvider, fConfig, fTools,
			prompt, name, r.model, r.notify
		};

		fWorker = spawn_thread(_RoutineWorkerEntry, "pluto-routine",
		                       B_NORMAL_PRIORITY, ctx);
		if (fWorker < 0) {
			delete ctx;
			fRunning = false;
			fRunBtn->SetLabel("Run now");
			fRunBtn->SetEnabled(true);
			BAlert* alert = new BAlert("Run routine",
				"Failed to create worker thread.",
				"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_STOP_ALERT);
			alert->Go();
			return;
		}
		resume_thread(fWorker);
		break;
	}
}

void RoutineDialog::_ToggleSelected() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;
	std::string name = fNames[sel];

	auto routines = ListRoutines();
	for (const auto& r : routines) {
		if (r.name != name) continue;
		std::string error;
		SetRoutineEnabled(r.path, !r.enabled, error);
		_RefreshList();
		break;
	}
}

void RoutineDialog::_ShowLog() {
	int32 sel = fList->CurrentSelection();
	if (sel < 0 || (size_t)sel >= fNames.size()) return;
	std::string name = fNames[sel];

	std::string log = ReadRoutineLog(name, 100);
	fLogView->SetText(log.c_str());
	fLogView->ScrollToOffset(fLogView->TextLength());
}

void RoutineDialog::_ImportRoutine() {
	fOpenPanel->Show();
}

void RoutineDialog::_HandleImportRef(BMessage* msg) {
	entry_ref ref;
	if (msg->FindRef("refs", &ref) != B_OK) return;

	BPath path(&ref);
	std::string filePath = path.Path();

	// Check extension.
	if (filePath.size() < 3 ||
	    filePath.substr(filePath.size() - 3) != ".md") {
		BAlert* alert = new BAlert("Import error",
			"Only .md files can be imported as routines.",
			"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
		return;
	}

	std::string error;
	if (!ImportRoutine(filePath, error)) {
		BAlert* alert = new BAlert("Import error",
			error.c_str(),
			"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
		return;
	}

	_RefreshList();

	// Parse imported routine to show confirmation.
	Routine r;
	ParseRoutine(filePath, r);
	BString info;
	info.SetToFormat("Routine \"%s\" imported.\n\nSchedule: %s\nEnabled: %s",
		r.name.c_str(), r.schedule.c_str(),
		r.enabled ? "yes" : "no");
	BAlert* alert = new BAlert("Import successful",
		info.String(),
		"OK", nullptr, nullptr, B_WIDTH_AS_USUAL, B_IDEA_ALERT);
	alert->Go();
}

}  // namespace gui
}  // namespace pluto
