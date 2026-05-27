#include "GraphWindow.h"
#include "GraphView.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <cstdio>

namespace pluto {
namespace gui {

static const uint32 kMsgSearch = 'grSr';
static const uint32 kMsgFitAll = 'grFA';

GraphWindow::GraphWindow(const map::Graph& graph)
	: BWindow(BRect(100, 100, 1000, 700),
	          "Pluto — Project Map",
	          B_TITLED_WINDOW,
	          B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	  fGraph(graph)
{
	// Graph viewer.
	fGraphView = new GraphView();
	fGraphView->SetGraph(graph);
	fGraphView->SetExplicitMinSize(BSize(400, 300));

	// Search bar.
	fSearchBar = new BTextControl("search", "Search:", "", nullptr);
	fSearchBar->SetModificationMessage(nullptr);
	fSearchBtn = new BButton("go", "Find",
		new BMessage(kMsgSearch));
	fFitAllBtn = new BButton("fitall", "Fit All",
		new BMessage(kMsgFitAll));

	// Stats label.
	char stats[128];
	snprintf(stats, sizeof(stats), "%zu nodes, %zu edges",
		graph.Nodes().size(), graph.Edges().size());
	fStatsLabel = new BStringView("stats", stats);

	// Detail panel.
	fDetailView = new BTextView("detail");
	fDetailView->SetWordWrap(true);
	fDetailView->MakeEditable(false);
	fDetailView->SetText(
		"Click a node to see its details.\n\n"
		"Drag nodes to rearrange.\n"
		"Scroll wheel to zoom.\n"
		"Drag background to pan.");
	fDetailView->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
	fDetailScroll = new BScrollView("detailScroll", fDetailView,
		0, false, true, B_FANCY_BORDER);

	// Layout: search bar on top, split view below (graph | detail).
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(8, 6, 8, 4)
			.Add(fSearchBar, 3.0f)
			.Add(fSearchBtn, 0)
			.Add(fFitAllBtn, 0)
			.AddGlue()
			.Add(fStatsLabel, 0)
		.End()
		.AddSplit(B_HORIZONTAL, 2, 1.0f)
			.Add(fGraphView, 3.0f)
			.Add(fDetailScroll, 1.0f)
		.End()
	.End();

	CenterOnScreen();
}

void GraphWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case GraphView::kMsgNodeSelected:
			_UpdateDetail(msg);
			break;
		case kMsgSearch:
			_DoSearch();
			break;
		case kMsgFitAll:
			fGraphView->FitAll();
			break;
		default:
			BWindow::MessageReceived(msg);
	}
}

bool GraphWindow::QuitRequested()
{
	return true;
}

void GraphWindow::_UpdateDetail(const BMessage* msg)
{
	const char* node_id = nullptr;
	const char* kind = nullptr;
	const char* label = nullptr;
	const char* file_path = nullptr;
	int32 line = 0, degree = 0;

	msg->FindString("node_id", &node_id);
	msg->FindString("kind", &kind);
	msg->FindString("label", &label);
	msg->FindString("file_path", &file_path);
	msg->FindInt32("line", &line);
	msg->FindInt32("degree", &degree);

	std::string text;
	text += "Node: ";
	text += label ? label : "(unknown)";
	text += "\n\n";

	text += "Kind: ";
	text += kind ? kind : "?";
	text += "\n";

	if (file_path && file_path[0]) {
		text += "File: ";
		text += file_path;
		if (line > 0) {
			text += ":";
			text += std::to_string(line);
		}
		text += "\n";
	}

	text += "Connections: ";
	text += std::to_string(degree);
	text += "\n\n";

	// List edges.
	if (node_id) {
		auto edges = fGraph.EdgesFor(node_id);
		if (!edges.empty()) {
			text += "Relationships:\n";
			for (const auto* e : edges) {
				std::string other = (e->source == node_id) ? e->target : e->source;
				std::string dir = (e->source == node_id) ? " -> " : " <- ";
				text += "  ";
				text += e->kind;
				text += dir;
				text += other;
				text += "\n";
			}
		}
	}

	fDetailView->SetText(text.c_str());
}

void GraphWindow::_DoSearch()
{
	const char* query = fSearchBar->Text();
	if (!query || !query[0]) {
		fGraphView->ClearHighlight();
		return;
	}

	auto found = fGraph.FindNodes(query);
	if (found.empty()) {
		fDetailView->SetText("No nodes found.");
		fGraphView->ClearHighlight();
		return;
	}

	std::vector<std::string> ids;
	std::string text = "Found " + std::to_string(found.size()) + " nodes:\n\n";
	for (const auto* n : found) {
		ids.push_back(n->id);
		text += "  " + n->label + " (" + n->kind + ")\n";
	}
	fDetailView->SetText(text.c_str());
	fGraphView->HighlightPath(ids);
}

}  // namespace gui
}  // namespace pluto
