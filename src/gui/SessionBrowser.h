#pragma once

#include <Window.h>

#include <string>
#include <vector>

class BButton;
class BListView;
class BStringView;
class BTextControl;

namespace pluto {
namespace gui {

// Message sent to parent window when user picks a session to load.
// Carries "path" string field.
constexpr uint32 kMsgSessionPicked = 'sbPk';

class SessionBrowser : public BWindow {
public:
	SessionBrowser(BRect frame, BWindow* parent);
	void MessageReceived(BMessage* msg) override;

private:
	void _RefreshList(const char* filter = nullptr);
	void _SelectionChanged();
	void _LoadSelected();
	void _DeleteSelected();

	BWindow*      fParent    = nullptr;
	BListView*    fList      = nullptr;
	BTextControl* fFilter    = nullptr;
	BStringView*  fDetail    = nullptr;
	BButton*      fLoadBtn   = nullptr;
	BButton*      fDeleteBtn = nullptr;

	// Parallel array of paths matching fList items.
	std::vector<std::string> fPaths;
};

}  // namespace gui
}  // namespace pluto
