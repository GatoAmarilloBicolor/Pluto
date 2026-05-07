#pragma once

#include <ListItem.h>
#include <Window.h>

#include <string>
#include <vector>

class BButton;
class BFilePanel;
class BListView;
class BMenuField;
class BSplitView;
class BTextControl;
class BTextView;

namespace pluto {
namespace gui {

// Custom two-line list item for schedules.
class ScheduleListItem : public BListItem {
public:
	ScheduleListItem(const std::string& name, const std::string& info,
	                 bool enabled);

	void DrawItem(BView* owner, BRect frame, bool complete) override;
	void Update(BView* owner, const BFont* font) override;

	const std::string& Name() const { return fName; }

private:
	std::string fName;
	std::string fInfo;
	bool fEnabled;
};

// Main schedule list dialog.
class ScheduleDialog : public BWindow {
public:
	ScheduleDialog(BRect frame);
	void MessageReceived(BMessage* msg) override;

private:
	void _RefreshList();
	void _SelectionChanged();
	void _AddSchedule();
	void _RemoveSelected();
	void _ToggleSelected();
	void _RunSelected();

	BListView*   fList       = nullptr;
	BTextView*   fDetailView = nullptr;
	BSplitView*  fSplitView  = nullptr;
	BButton*     fAddBtn     = nullptr;
	BButton*     fRemoveBtn  = nullptr;
	BButton*     fToggleBtn  = nullptr;
	BButton*     fRunBtn     = nullptr;

	std::vector<std::string> fNames;
};

// Add/edit schedule form dialog.
constexpr uint32 kMsgScheduleCreated = 'scCr';

class AddScheduleDialog : public BWindow {
public:
	AddScheduleDialog(BRect frame, BWindow* parent);
	~AddScheduleDialog() override;
	void MessageReceived(BMessage* msg) override;

private:
	BWindow*      fParent      = nullptr;
	BTextControl* fName        = nullptr;
	BTextControl* fPromptFile  = nullptr;
	BTextControl* fInterval    = nullptr;
	BTextControl* fModel       = nullptr;
	BMenuField*   fEventField  = nullptr;
	BFilePanel*   fFilePanel   = nullptr;
};

}  // namespace gui
}  // namespace pluto
