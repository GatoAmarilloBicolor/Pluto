#include "MiniMapView.h"
#include "ChatColors.h"

#include <ScrollBar.h>

namespace pluto {
namespace gui {

static const float kMiniMapWidth = 16.0f;

MiniMapView::MiniMapView()
	: BView("minimap", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE) {
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
}

void MiniMapView::SetSegments(const std::vector<Segment>& segs) {
	fSegments = segs;
	Invalidate();
}

void MiniMapView::SetTarget(BView* tv, BScrollBar* sb) {
	fTarget = tv;
	fScrollBar = sb;
}

void MiniMapView::Draw(BRect /*updateRect*/) {
	BRect bounds = Bounds();
	float h = bounds.Height();

	// Background.
	SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	FillRect(bounds);

	// Draw each segment as a colored rectangle.
	for (const auto& seg : fSegments) {
		rgb_color c;
		if (seg.role == "user")
			c = (rgb_color){80, 130, 220, 255};   // blue
		else if (seg.role == "assistant")
			c = (rgb_color){80, 180, 100, 255};    // green
		else
			c = (rgb_color){220, 150, 60, 255};    // orange (tool)

		float y1 = bounds.top + seg.start * h;
		float y2 = y1 + std::max(seg.length * h, 2.0f);
		BRect r(bounds.left + 2, y1, bounds.right - 2, y2);
		SetHighColor(c);
		FillRect(r);
	}

	// Viewport indicator: a translucent overlay showing the visible area.
	if (fScrollBar && fTarget) {
		float min, max;
		fScrollBar->GetRange(&min, &max);
		if (max > min) {
			float val = fScrollBar->Value();
			float proportion = fTarget->Bounds().Height() / (max - min + fTarget->Bounds().Height());
			float pos = (val - min) / (max - min + fTarget->Bounds().Height());
			float vh = proportion * h;
			float vy = pos * h;
			SetHighColor((rgb_color){128, 128, 128, 60});
			SetDrawingMode(B_OP_ALPHA);
			FillRect(BRect(bounds.left, vy, bounds.right, vy + vh));
			SetDrawingMode(B_OP_COPY);
		}
	}
}

void MiniMapView::MouseDown(BPoint where) {
	if (fSegments.empty() || !fScrollBar) return;

	float h = Bounds().Height();
	if (h <= 0) return;
	float frac = where.y / h;

	// Find the segment closest to the click.
	float bestDist = 999.0f;
	float scrollY = 0;
	for (const auto& seg : fSegments) {
		float mid = seg.start + seg.length * 0.5f;
		float d = std::abs(mid - frac);
		if (d < bestDist) {
			bestDist = d;
			scrollY = seg.scroll_y;
		}
	}

	fScrollBar->SetValue(scrollY);
}

BSize MiniMapView::MinSize() { return BSize(kMiniMapWidth, 50); }
BSize MiniMapView::MaxSize() { return BSize(kMiniMapWidth, B_SIZE_UNLIMITED); }
BSize MiniMapView::PreferredSize() { return BSize(kMiniMapWidth, 200); }

}  // namespace gui
}  // namespace pluto
