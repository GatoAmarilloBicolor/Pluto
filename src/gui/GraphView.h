#pragma once

#include <View.h>

#include <set>
#include <string>

#include "../map/Graph.h"

namespace pluto {
namespace gui {

// Native BView for interactive knowledge graph visualization.
// Nodes are colored by kind, edges drawn as lines with arrows.
// Supports drag to move nodes, pan, zoom, and click to select.
class GraphView : public BView {
public:
	GraphView();

	void SetGraph(const pluto::map::Graph& graph);
	void FitAll();
	void HighlightPath(const std::vector<std::string>& node_ids);
	void ClearHighlight();

	// Returns the ID of the currently selected node, or empty.
	std::string SelectedNodeId() const;

	// BView overrides.
	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseUp(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit,
	                const BMessage* msg) override;
	void MessageReceived(BMessage* msg) override;
	void FrameResized(float width, float height) override;

	static const uint32 kMsgNodeSelected = 'gvNS';

private:
	BPoint _GraphToScreen(float gx, float gy) const;
	BPoint _ScreenToGraph(BPoint screen) const;
	int    _HitTest(BPoint screen) const;

	void _DrawEdge(const pluto::map::GraphEdge& edge);
	void _DrawNode(int index, bool selected, bool highlighted);

	rgb_color _ColorForKind(const std::string& kind) const;

	pluto::map::Graph fGraph;
	float  fZoom = 1.0f;
	BPoint fPan = {0, 0};
	int    fDragNode = -1;
	BPoint fDragStart;
	bool   fPanning = false;
	BPoint fPanStart;
	int    fSelectedNode = -1;
	int    fHoverNode = -1;
	BPoint fHoverScreenPos;
	bool   fNeedsFitAll = false;
	std::set<std::string> fHighlighted;
};

}  // namespace gui
}  // namespace pluto
