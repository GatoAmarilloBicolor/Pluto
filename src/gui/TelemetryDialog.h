#pragma once

#include <Window.h>
#include <View.h>

#include <vector>

#include "../telemetry.h"

namespace pluto {
namespace gui {

// Bar chart view showing token usage per model.
class ChartView : public BView {
public:
	ChartView();
	void SetData(const std::vector<ModelStats>& stats);
	void Draw(BRect updateRect) override;
	BSize MinSize() override;
	BSize PreferredSize() override;

private:
	std::vector<ModelStats> fStats;
};

// Dialog showing telemetry statistics with a bar chart.
class TelemetryDialog : public BWindow {
public:
	TelemetryDialog();
};

}  // namespace gui
}  // namespace pluto
