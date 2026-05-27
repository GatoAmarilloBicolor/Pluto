#include "GraphView.h"

#include <Font.h>
#include <Message.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

namespace pluto {
namespace gui {

static const float kNodeRadius = 24.0f;
static const float kHitRadius  = 28.0f;
static const float kArrowSize  = 10.0f;
static const float kZoomStep   = 1.12f;
static const float kMinZoom    = 0.05f;
static const float kMaxZoom    = 8.0f;

GraphView::GraphView()
	: BView("graphview",
	        B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_FRAME_EVENTS)
{
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
}

void GraphView::SetGraph(const map::Graph& graph)
{
	fGraph = graph;
	fSelectedNode = -1;
	fDragNode = -1;
	fHighlighted.clear();

	// Fit the graph into the view with some padding.
	if (!fGraph.Nodes().empty()) {
		float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
		for (const auto& n : fGraph.Nodes()) {
			minX = std::min(minX, n.x);
			minY = std::min(minY, n.y);
			maxX = std::max(maxX, n.x);
			maxY = std::max(maxY, n.y);
		}
		float gw = maxX - minX;
		float gh = maxY - minY;
		if (gw < 1) gw = 1;
		if (gh < 1) gh = 1;

		BRect b = Bounds();
		float padding = 80;
		float zx = (b.Width() - padding * 2) / gw;
		float zy = (b.Height() - padding * 2) / gh;
		fZoom = std::min(zx, zy);
		fZoom = std::max(kMinZoom, std::min(2.0f, fZoom));

		float cx = (minX + maxX) / 2;
		float cy = (minY + maxY) / 2;
		fPan.x = b.Width() / 2 - cx * fZoom;
		fPan.y = b.Height() / 2 - cy * fZoom;
	}
	Invalidate();
}

void GraphView::HighlightPath(const std::vector<std::string>& node_ids)
{
	fHighlighted.clear();
	for (const auto& id : node_ids)
		fHighlighted.insert(id);
	Invalidate();
}

void GraphView::ClearHighlight()
{
	fHighlighted.clear();
	Invalidate();
}

std::string GraphView::SelectedNodeId() const
{
	if (fSelectedNode >= 0 && fSelectedNode < (int)fGraph.Nodes().size())
		return fGraph.Nodes()[fSelectedNode].id;
	return {};
}

// ---- Coordinate transforms ----

BPoint GraphView::_GraphToScreen(float gx, float gy) const
{
	return BPoint(gx * fZoom + fPan.x, gy * fZoom + fPan.y);
}

BPoint GraphView::_ScreenToGraph(BPoint screen) const
{
	return BPoint((screen.x - fPan.x) / fZoom,
	              (screen.y - fPan.y) / fZoom);
}

int GraphView::_HitTest(BPoint screen) const
{
	const auto& nodes = fGraph.Nodes();
	for (int i = (int)nodes.size() - 1; i >= 0; --i) {
		BPoint sp = _GraphToScreen(nodes[i].x, nodes[i].y);
		float dx = screen.x - sp.x;
		float dy = screen.y - sp.y;
		float hr = kHitRadius * std::max(0.5f, fZoom);
		if (dx * dx + dy * dy <= hr * hr)
			return i;
	}
	return -1;
}

// ---- Colors ----

rgb_color GraphView::_ColorForKind(const std::string& kind) const
{
	if (kind == "class")     return {46, 160, 67, 255};     // green
	if (kind == "function")  return {218, 145, 32, 255};    // amber
	if (kind == "header")    return {88, 140, 210, 255};    // steel blue
	if (kind == "namespace") return {162, 85, 190, 255};    // purple
	if (kind == "module")    return {190, 75, 75, 255};     // crimson
	return {58, 120, 200, 255};                             // blue (file)
}

static rgb_color _ColorForEdgeKind(const std::string& kind)
{
	if (kind == "includes")    return {80, 140, 200, 255};   // blue
	if (kind == "calls")       return {200, 130, 40, 255};   // orange
	if (kind == "defines")     return {70, 160, 80, 255};    // green
	if (kind == "inherits")    return {180, 70, 180, 255};   // purple
	if (kind == "depends_on")  return {160, 80, 80, 255};    // red
	return {120, 120, 140, 255};                             // grey
}

// ---- Drawing ----

void GraphView::_DrawEdge(const map::GraphEdge& edge)
{
	const auto& nodes = fGraph.Nodes();
	int si = -1, ti = -1;
	for (int i = 0; i < (int)nodes.size(); ++i) {
		if (nodes[i].id == edge.source) si = i;
		if (nodes[i].id == edge.target) ti = i;
		if (si >= 0 && ti >= 0) break;
	}
	if (si < 0 || ti < 0) return;

	BPoint sp = _GraphToScreen(nodes[si].x, nodes[si].y);
	BPoint tp = _GraphToScreen(nodes[ti].x, nodes[ti].y);

	bool bright = fHighlighted.count(edge.source)
	           && fHighlighted.count(edge.target);
	if (fSelectedNode >= 0
	    && (si == fSelectedNode || ti == fSelectedNode))
		bright = true;

	rgb_color edgeColor = _ColorForEdgeKind(edge.kind);

	if (bright) {
		SetHighColor(edgeColor);
		SetPenSize(std::max(1.5f, 2.0f * fZoom));
	} else {
		// Soften unselected edges.
		edgeColor.red   = (edgeColor.red + 200) / 2;
		edgeColor.green = (edgeColor.green + 200) / 2;
		edgeColor.blue  = (edgeColor.blue + 200) / 2;
		edgeColor.alpha = 140;
		SetHighColor(edgeColor);
		SetPenSize(std::max(0.8f, 1.0f * fZoom));
	}

	// Draw a Bezier curve instead of a straight line for a smoother look.
	float mx = (sp.x + tp.x) / 2;
	float my = (sp.y + tp.y) / 2;
	// Offset the midpoint perpendicular to the line for a slight curve.
	float dx = tp.x - sp.x;
	float dy = tp.y - sp.y;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len < 1.0f) return;
	float curveAmount = std::min(20.0f * fZoom, len * 0.1f);
	float nx = -dy / len * curveAmount;
	float ny =  dx / len * curveAmount;

	BPoint ctrl1 = {mx + nx, my + ny};
	BPoint points[4] = {sp, ctrl1, ctrl1, tp};
	StrokeBezier(points);

	// Arrowhead at target.
	// Compute direction from the last curve segment toward target.
	float adx = tp.x - ctrl1.x;
	float ady = tp.y - ctrl1.y;
	float alen = std::sqrt(adx * adx + ady * ady);
	if (alen < 1.0f) { adx = dx; ady = dy; alen = len; }
	adx /= alen;
	ady /= alen;

	float r = kNodeRadius * fZoom * 0.5f;
	BPoint arrow_tip = {tp.x - adx * r, tp.y - ady * r};

	float ax = kArrowSize * fZoom * 0.35f;
	BPoint left  = {arrow_tip.x - adx * ax + ady * ax * 0.5f,
	                arrow_tip.y - ady * ax - adx * ax * 0.5f};
	BPoint right = {arrow_tip.x - adx * ax - ady * ax * 0.5f,
	                arrow_tip.y - ady * ax + adx * ax * 0.5f};
	FillTriangle(arrow_tip, left, right);

	SetPenSize(1.0f);
}

void GraphView::_DrawNode(int index, bool selected, bool highlighted)
{
	const auto& node = fGraph.Nodes()[index];
	BPoint center = _GraphToScreen(node.x, node.y);
	float r = kNodeRadius * fZoom * 0.5f;
	if (r < 3.0f) r = 3.0f;

	rgb_color fill = _ColorForKind(node.kind);

	if (highlighted) {
		fill.red   = std::min(255, fill.red + 50);
		fill.green = std::min(255, fill.green + 50);
		fill.blue  = std::min(255, fill.blue + 50);
	}

	// Shadow.
	if (fZoom >= 0.3f) {
		SetHighColor(0, 0, 0, 40);
		FillEllipse(BPoint(center.x + 2, center.y + 2), r, r);
	}

	// Fill circle.
	SetHighColor(fill);
	FillEllipse(center, r, r);

	// Border.
	if (selected) {
		SetHighColor(255, 210, 40, 255);  // gold
		SetPenSize(3.0f);
		StrokeEllipse(center, r + 1, r + 1);
	} else if (highlighted) {
		SetHighColor(255, 255, 255, 200);
		SetPenSize(2.0f);
		StrokeEllipse(center, r, r);
	} else {
		// Subtle darker border.
		rgb_color border = {
			(uint8)std::max(0, fill.red - 40),
			(uint8)std::max(0, fill.green - 40),
			(uint8)std::max(0, fill.blue - 40),
			200
		};
		SetHighColor(border);
		SetPenSize(1.0f);
		StrokeEllipse(center, r, r);
	}
	SetPenSize(1.0f);

	// Label — only when zoomed in enough to read.
	if (fZoom >= 0.3f) {
		BFont font;
		GetFont(&font);
		float fontSize = std::max(9.0f, 11.0f * fZoom);
		fontSize = std::min(fontSize, 16.0f);
		font.SetSize(fontSize);
		SetFont(&font);

		std::string label = node.label;
		auto slash = label.rfind('/');
		if (slash != std::string::npos)
			label = label.substr(slash + 1);
		if (label.size() > 24)
			label = label.substr(0, 22) + "\xe2\x80\xa6";  // ellipsis

		float tw = StringWidth(label.c_str());
		BPoint textPos(center.x - tw / 2, center.y + r + fontSize + 3);

		// Background pill for readability.
		float pillPad = 3;
		BRect pill(textPos.x - pillPad,
		           textPos.y - fontSize + 1,
		           textPos.x + tw + pillPad,
		           textPos.y + 3);
		SetHighColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
		pill.left -= 1;
		pill.right += 1;
		FillRoundRect(pill, 3, 3);

		// Text.
		if (selected || highlighted)
			SetHighColor(30, 30, 30, 255);
		else
			SetHighColor(60, 60, 60, 255);
		DrawString(label.c_str(), textPos);
	}
}

void GraphView::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();
	SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	FillRect(bounds);

	if (fGraph.Nodes().empty()) {
		SetHighColor(120, 120, 120, 255);
		BFont font;
		GetFont(&font);
		font.SetSize(14);
		SetFont(&font);
		const char* msg = "No graph loaded. Use /map to scan the workspace.";
		float tw = StringWidth(msg);
		DrawString(msg, BPoint(bounds.Width() / 2 - tw / 2,
		                       bounds.Height() / 2));
		return;
	}

	// Draw edges first (behind nodes).
	for (const auto& e : fGraph.Edges())
		_DrawEdge(e);

	// Draw nodes on top.
	for (int i = 0; i < (int)fGraph.Nodes().size(); ++i) {
		bool sel = (i == fSelectedNode);
		bool hl = fHighlighted.count(fGraph.Nodes()[i].id) > 0;
		_DrawNode(i, sel, hl);
	}

	// Legend (bottom-left).
	{
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);

		const struct { const char* label; rgb_color color; } legend[] = {
			{"file",      {58, 120, 200, 255}},
			{"header",    {88, 140, 210, 255}},
			{"class",     {46, 160, 67, 255}},
			{"function",  {218, 145, 32, 255}},
			{"namespace", {162, 85, 190, 255}},
		};

		// Background pill for legend.
		float lx = bounds.left + 8;
		float ly = bounds.bottom - 8;
		float legendH = 5 * 16 + 8;
		BRect legendBg(lx - 4, ly - legendH, lx + 90, ly + 4);
		SetHighColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
		FillRoundRect(legendBg, 4, 4);
		SetHighColor(180, 180, 180, 100);
		StrokeRoundRect(legendBg, 4, 4);

		for (int i = 4; i >= 0; --i) {
			SetHighColor(legend[i].color);
			FillRoundRect(BRect(lx, ly - 10, lx + 12, ly - 1), 2, 2);
			SetHighColor(80, 80, 80, 255);
			DrawString(legend[i].label, BPoint(lx + 16, ly - 1));
			ly -= 16;
		}
	}

	// Edge legend (bottom-right).
	if (fZoom >= 0.4f) {
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);

		const struct { const char* label; rgb_color color; } edgeLegend[] = {
			{"includes",   {80, 140, 200, 255}},
			{"calls",      {200, 130, 40, 255}},
			{"defines",    {70, 160, 80, 255}},
			{"inherits",   {180, 70, 180, 255}},
			{"depends_on", {160, 80, 80, 255}},
		};

		float rx = bounds.right - 100;
		float ry = bounds.bottom - 8;
		float legendH = 5 * 16 + 8;
		BRect legendBg(rx - 4, ry - legendH, rx + 96, ry + 4);
		SetHighColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
		FillRoundRect(legendBg, 4, 4);
		SetHighColor(180, 180, 180, 100);
		StrokeRoundRect(legendBg, 4, 4);

		for (int i = 4; i >= 0; --i) {
			SetHighColor(edgeLegend[i].color);
			SetPenSize(2.0f);
			StrokeLine(BPoint(rx, ry - 5), BPoint(rx + 14, ry - 5));
			SetPenSize(1.0f);
			SetHighColor(80, 80, 80, 255);
			DrawString(edgeLegend[i].label, BPoint(rx + 18, ry - 1));
			ry -= 16;
		}
	}

	// Stats (top-left).
	{
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);

		char stats[128];
		snprintf(stats, sizeof(stats), "%zu nodes, %zu edges  \xe2\x80\x94  scroll to zoom, drag to pan",
			fGraph.Nodes().size(), fGraph.Edges().size());

		float tw = StringWidth(stats);
		BRect bg(bounds.left + 6, bounds.top + 4,
		         bounds.left + tw + 16, bounds.top + 20);
		SetHighColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
		FillRoundRect(bg, 3, 3);

		SetHighColor(100, 100, 100, 255);
		DrawString(stats, BPoint(bounds.left + 10, bounds.top + 16));
	}
}

// ---- Mouse interaction ----

void GraphView::MouseDown(BPoint where)
{
	MakeFocus(true);

	int hit = _HitTest(where);
	if (hit >= 0) {
		fDragNode = hit;
		fDragStart = where;
		fSelectedNode = hit;

		if (Window()) {
			BMessage msg(kMsgNodeSelected);
			msg.AddString("node_id", fGraph.Nodes()[hit].id.c_str());
			msg.AddString("kind", fGraph.Nodes()[hit].kind.c_str());
			msg.AddString("label", fGraph.Nodes()[hit].label.c_str());
			msg.AddString("file_path", fGraph.Nodes()[hit].file_path.c_str());
			msg.AddInt32("line", fGraph.Nodes()[hit].line);
			int degree = (int)fGraph.EdgesFor(fGraph.Nodes()[hit].id).size();
			msg.AddInt32("degree", degree);
			Window()->PostMessage(&msg);
		}
		Invalidate();
	} else {
		fPanning = true;
		fPanStart = where;
		fSelectedNode = -1;
		Invalidate();
	}

	SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
}

void GraphView::MouseUp(BPoint /*where*/)
{
	fDragNode = -1;
	fPanning = false;
}

void GraphView::MouseMoved(BPoint where, uint32 /*transit*/,
                           const BMessage* /*msg*/)
{
	if (fDragNode >= 0) {
		BPoint gp = _ScreenToGraph(where);
		auto& nodes = const_cast<std::vector<map::GraphNode>&>(fGraph.Nodes());
		nodes[fDragNode].x = gp.x;
		nodes[fDragNode].y = gp.y;
		Invalidate();
	} else if (fPanning) {
		float dx = where.x - fPanStart.x;
		float dy = where.y - fPanStart.y;
		fPan.x += dx;
		fPan.y += dy;
		fPanStart = where;
		Invalidate();
	}
}

void GraphView::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case B_MOUSE_WHEEL_CHANGED: {
			float dy = 0;
			msg->FindFloat("be:wheel_delta_y", &dy);
			if (dy == 0) break;

			BPoint mouse;
			uint32 buttons;
			GetMouse(&mouse, &buttons, false);

			BPoint before = _ScreenToGraph(mouse);
			fZoom *= (dy < 0) ? kZoomStep : (1.0f / kZoomStep);
			fZoom = std::max(kMinZoom, std::min(kMaxZoom, fZoom));
			BPoint after = _GraphToScreen(before.x, before.y);

			fPan.x += mouse.x - after.x;
			fPan.y += mouse.y - after.y;
			Invalidate();
			break;
		}
		default:
			BView::MessageReceived(msg);
	}
}

void GraphView::FrameResized(float /*width*/, float /*height*/)
{
	Invalidate();
}

}  // namespace gui
}  // namespace pluto
