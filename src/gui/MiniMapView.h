#pragma once

#include <View.h>

#include <string>
#include <vector>

class BScrollBar;

namespace pluto {
namespace gui {

// A narrow sidebar showing colored blocks for each message in the
// conversation: blue for user, green for assistant, orange for tool.
// Clicking a block scrolls the output to the corresponding offset.
class MiniMapView : public BView {
public:
	MiniMapView();

	// A single segment in the minimap.
	struct Segment {
		std::string role;   // "user", "assistant", "tool"
		float       start;  // fractional position [0..1]
		float       length; // fractional length
		float       scroll_y; // target scroll Y position in the text view
	};

	// Rebuild the segment list from the text view. Call after each turn.
	void SetSegments(const std::vector<Segment>& segs);

	// Set the view to scroll on click.
	void SetTarget(BView* view, BScrollBar* sb);

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	BSize MinSize() override;
	BSize MaxSize() override;
	BSize PreferredSize() override;

private:
	std::vector<Segment> fSegments;
	BView*      fTarget = nullptr;
	BScrollBar* fScrollBar = nullptr;
};

}  // namespace gui
}  // namespace pluto
