#pragma once

#include <Window.h>

#include "../map/Graph.h"

class BStringView;
class BTextControl;
class BTextView;
class BScrollView;
class BButton;

namespace pluto {
namespace gui {

class GraphView;

// Standalone window hosting the interactive graph viewer.
// Opened from /map in the GUI or via File menu.
class GraphWindow : public BWindow {
public:
	GraphWindow(const pluto::map::Graph& graph);

	void MessageReceived(BMessage* msg) override;
	bool QuitRequested() override;

private:
	void _UpdateDetail(const BMessage* msg);
	void _DoSearch();

	GraphView*    fGraphView = nullptr;
	BTextControl* fSearchBar = nullptr;
	BButton*      fSearchBtn = nullptr;
	BButton*      fFitAllBtn = nullptr;
	BStringView*  fStatsLabel = nullptr;
	BStringView*  fHintLabel = nullptr;
	BTextView*    fDetailView = nullptr;
	BScrollView*  fDetailScroll = nullptr;

	pluto::map::Graph fGraph;
};

}  // namespace gui
}  // namespace pluto
