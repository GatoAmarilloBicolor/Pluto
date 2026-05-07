// Tracker add-on "Ask Pluto" — adds a context menu entry in Tracker.
//
// When invoked on selected files, it sends them to Pluto-gui via
// BMessage. If Pluto-gui is already running, the files are delivered
// to the existing instance; otherwise Pluto-gui is launched first.
//
// Build:   g++ -shared -fPIC -o "Ask Pluto" TrackerAddOn.cpp -lbe
// Install: cp "Ask Pluto" ~/config/non-packaged/add-ons/Tracker/

#include <Directory.h>
#include <Entry.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>

#include <cstdlib>
#include <string>

static const char* kPlutoSig = "application/x-vnd.Hay-pluto";

// Custom message: deliver file refs with a prompt to the main window.
// Pluto-gui handles this in MainWindow::MessageReceived as B_REFS_RECEIVED.
static const uint32 kMsgAskPluto = 'AsPl';

extern "C" void
process_refs(entry_ref dir, BMessage* message, void* /*reserved*/)
{
	// Resolve the Tracker directory where the add-on was invoked.
	BPath dir_path;
	{
		BEntry dir_entry(&dir);
		if (dir_entry.InitCheck() == B_OK)
			dir_entry.GetPath(&dir_path);
	}

	// Collect refs.
	BMessage forward(B_REFS_RECEIVED);
	entry_ref ref;
	entry_ref first_ref;
	int count = 0;
	std::string names;
	std::string mentions;
	for (int i = 0; message->FindRef("refs", i, &ref) == B_OK; ++i) {
		if (i == 0) first_ref = ref;
		forward.AddRef("refs", &ref);
		BPath path(&ref);
		if (path.InitCheck() == B_OK) {
			const char* leaf = path.Leaf();
			if (count > 0) names += ", ";
			names += leaf ? leaf : path.Path();
			mentions += " @";
			mentions += path.Path();
		}
		count++;
	}
	// Determine workspace: if a single directory is selected, use it
	// as the workspace; otherwise use the Tracker folder.
	std::string workspace;
	if (count == 1) {
		BEntry sel_entry(&first_ref);
		if (sel_entry.InitCheck() == B_OK && sel_entry.IsDirectory()) {
			BPath sel_path;
			sel_entry.GetPath(&sel_path);
			if (sel_path.InitCheck() == B_OK)
				workspace = sel_path.Path();
		}
	}
	if (workspace.empty() && dir_path.InitCheck() == B_OK)
		workspace = dir_path.Path();

	if (!workspace.empty())
		forward.AddString("ask_pluto_cwd", workspace.c_str());

	// Build a prompt.
	std::string prompt;
	if (count > 0) {
		prompt = "The user selected these files in Tracker: " + names + "."
			+ mentions
			+ "\nWorkspace: " + (workspace.empty() ? "(unknown)" : workspace)
			+ "\nWhat would you like me to do with them?";
	} else {
		// No files selected — opened from background of a Tracker window.
		prompt = "The user opened Ask Pluto from the Tracker folder: "
			+ (workspace.empty() ? "(unknown)" : workspace)
			+ "\nWorkspace: " + (workspace.empty() ? "(unknown)" : workspace)
			+ "\nHow can I help with this project?";
	}
	forward.AddString("ask_pluto_prompt", prompt.c_str());

	if (!be_roster->IsRunning(kPlutoSig)) {
		// Launch Pluto with --workspace so it initializes to the correct
		// directory before any trust prompt or config loading.
		std::string ws_arg = "--workspace=";
		ws_arg += workspace.empty() ? "." : workspace;
		const char* argv[] = { ws_arg.c_str(), nullptr };
		be_roster->Launch(kPlutoSig, 1, const_cast<char**>(argv));
		// Wait for the app to start and create its window.
		snooze(1500000);  // 1.5s
	}

	// Send refs + prompt to the running instance.
	BMessenger target(kPlutoSig);
	if (target.IsValid())
		target.SendMessage(&forward);
}
