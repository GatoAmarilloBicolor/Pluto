#pragma once

#include <ListItem.h>
#include <Window.h>

#include <string>
#include <vector>

class BButton;
class BFilePanel;
class BListView;
class BSplitView;
class BTextControl;
class BTextView;

namespace pluto {
namespace gui {

// Custom list item that renders agent name (bold) + description (dim)
// on two lines, with version badge on the right.
class AgentListItem : public BListItem {
public:
	AgentListItem(const std::string& name, const std::string& version,
	              const std::string& description, const std::string& author);

	void DrawItem(BView* owner, BRect frame, bool complete) override;
	void Update(BView* owner, const BFont* font) override;

	const std::string& Name() const { return fName; }
	const std::string& Version() const { return fVersion; }
	const std::string& Description() const { return fDescription; }
	const std::string& Author() const { return fAuthor; }

private:
	std::string fName;
	std::string fVersion;
	std::string fDescription;
	std::string fAuthor;
};

class AgentStoreDialog : public BWindow {
public:
	AgentStoreDialog(BRect frame);
	~AgentStoreDialog() override;
	void MessageReceived(BMessage* msg) override;

private:
	void _RefreshList();
	void _SelectionChanged();
	void _InstallFromPath();
	void _InstallWithReview(const char* source);
	void _RemoveSelected();
	void _UpdateSelected();
	void _UpdateTitle();

	BListView*   fList         = nullptr;
	BTextView*   fDetailView   = nullptr;
	BTextView*   fPromptView   = nullptr;
	BSplitView*  fSplitView    = nullptr;
	BTextControl* fSearchField  = nullptr;
	BButton*     fInstallBtn   = nullptr;
	BButton*     fRemoveBtn    = nullptr;
	BButton*     fUpdateBtn    = nullptr;
	BFilePanel*  fFilePanel    = nullptr;

	std::string fSearchQuery;         // current filter
	std::vector<std::string> fNames;  // parallel to list items
};

}  // namespace gui
}  // namespace pluto
