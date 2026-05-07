#pragma once

#include <FilePanel.h>
#include <ListItem.h>
#include <Window.h>

#include <string>
#include <vector>

#include "../config.h"
#include "../provider/Provider.h"
#include "../tools/Tool.h"

class BButton;
class BListView;
class BSplitView;
class BTextView;

namespace pluto {
namespace gui {

// Custom two-line list item for routines.
class RoutineListItem : public BListItem {
public:
	RoutineListItem(const std::string& name, const std::string& schedule,
	                bool enabled);

	void DrawItem(BView* owner, BRect frame, bool complete) override;
	void Update(BView* owner, const BFont* font) override;

	const std::string& Name() const { return fName; }

private:
	std::string fName;
	std::string fSchedule;
	bool fEnabled;
};

class RoutineDialog : public BWindow {
public:
	RoutineDialog(BRect frame,
	              pluto::Provider* provider = nullptr,
	              pluto::Config* config = nullptr,
	              pluto::ToolRegistry* tools = nullptr);
	~RoutineDialog() override;
	void MessageReceived(BMessage* msg) override;

private:
	void _RefreshList();
	void _SelectionChanged();
	void _RunSelected();
	void _ToggleSelected();
	void _ShowLog();
	void _ImportRoutine();
	void _HandleImportRef(BMessage* msg);

	BListView*   fList       = nullptr;
	BTextView*   fDetailView = nullptr;
	BTextView*   fLogView    = nullptr;
	BSplitView*  fSplitView  = nullptr;
	BButton*     fRunBtn     = nullptr;
	BButton*     fToggleBtn  = nullptr;
	BButton*     fLogBtn     = nullptr;
	BButton*     fImportBtn  = nullptr;
	BFilePanel*  fOpenPanel  = nullptr;

	pluto::Provider*     fProvider = nullptr;
	pluto::Config*       fConfig   = nullptr;
	pluto::ToolRegistry* fTools    = nullptr;
	thread_id            fWorker   = -1;
	bool                 fRunning  = false;
	std::string          fRunOutput;  // accumulated output for logging

	std::vector<std::string> fNames;
};

}  // namespace gui
}  // namespace pluto
