#include "TelemetryDialog.h"

#include <Font.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>

#include <algorithm>
#include <cstdio>

namespace pluto {
namespace gui {

// Assign distinct colors to models.
static const rgb_color kBarColors[] = {
	{  66, 133, 244, 255 },  // blue
	{  52, 168,  83, 255 },  // green
	{ 234,  67,  53, 255 },  // red
	{ 251, 188,   4, 255 },  // yellow
	{ 156,  39, 176, 255 },  // purple
	{   0, 172, 193, 255 },  // cyan
	{ 255, 112,  67, 255 },  // orange
	{ 121,  85,  72, 255 },  // brown
};
static const int kNumColors = sizeof(kBarColors) / sizeof(kBarColors[0]);

// ---- ChartView ----

ChartView::ChartView()
	: BView("chart", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE) {
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
}

void ChartView::SetData(const std::vector<ModelStats>& stats) {
	fStats = stats;
	// Sort by total tokens descending.
	std::sort(fStats.begin(), fStats.end(), [](const ModelStats& a, const ModelStats& b) {
		return (a.input_tokens + a.output_tokens) > (b.input_tokens + b.output_tokens);
	});
	Invalidate();
}

void ChartView::Draw(BRect /*updateRect*/) {
	BRect bounds = Bounds();
	SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	FillRect(bounds);

	if (fStats.empty()) {
		SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
		DrawString("No telemetry data.", BPoint(20, 30));
		return;
	}

	const float kLeftMargin = 160;
	const float kRightMargin = 80;
	const float kRowHeight = 28;
	const float kBarHeight = 18;
	const float kTopMargin = 10;
	float barAreaWidth = bounds.Width() - kLeftMargin - kRightMargin;
	if (barAreaWidth < 50) barAreaWidth = 50;

	int64_t maxTok = 0;
	for (const auto& s : fStats)
		maxTok = std::max(maxTok, s.input_tokens + s.output_tokens);
	if (maxTok == 0) maxTok = 1;

	BFont font = *be_plain_font;
	font.SetSize(11);
	SetFont(&font);

	for (size_t i = 0; i < fStats.size(); ++i) {
		const auto& s = fStats[i];
		float y = kTopMargin + i * kRowHeight;

		// Model name.
		SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
		std::string name = s.model;
		if (name.size() > 22) name = name.substr(0, 19) + "\xe2\x80\xa6";
		DrawString(name.c_str(), BPoint(8, y + kBarHeight - 4));

		// Input bar.
		float inW = (float)s.input_tokens / maxTok * barAreaWidth;
		float outW = (float)s.output_tokens / maxTok * barAreaWidth;
		if (inW < 1 && s.input_tokens > 0) inW = 1;
		if (outW < 1 && s.output_tokens > 0) outW = 1;

		rgb_color c = kBarColors[i % kNumColors];
		SetHighColor(c);
		FillRect(BRect(kLeftMargin, y, kLeftMargin + inW, y + kBarHeight));

		// Output bar (slightly lighter).
		rgb_color c2 = c;
		c2.red = std::min(255, c2.red + 50);
		c2.green = std::min(255, c2.green + 50);
		c2.blue = std::min(255, c2.blue + 50);
		SetHighColor(c2);
		FillRect(BRect(kLeftMargin + inW, y,
		               kLeftMargin + inW + outW, y + kBarHeight));

		// Token count label.
		SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
		char label[64];
		int64_t total = s.input_tokens + s.output_tokens;
		if (total >= 1000000)
			snprintf(label, sizeof(label), "%.1fM", total / 1e6);
		else if (total >= 1000)
			snprintf(label, sizeof(label), "%.1fK", total / 1e3);
		else
			snprintf(label, sizeof(label), "%lld", (long long)total);
		DrawString(label, BPoint(kLeftMargin + inW + outW + 6, y + kBarHeight - 4));
	}

	// Legend at the bottom.
	float legendY = kTopMargin + fStats.size() * kRowHeight + 15;
	SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
	font.SetSize(10);
	SetFont(&font);
	DrawString("Darker = input tokens, lighter = output tokens",
		BPoint(kLeftMargin, legendY));

	// Cost summary.
	double total_cost = 0;
	for (const auto& s : fStats) total_cost += s.total_cost_usd;
	char costStr[64];
	snprintf(costStr, sizeof(costStr), "Total estimated cost: $%.4f", total_cost);
	DrawString(costStr, BPoint(kLeftMargin, legendY + 16));
}

BSize ChartView::MinSize() {
	float h = 30 + fStats.size() * 28 + 50;
	return BSize(400, std::max(h, 120.0f));
}

BSize ChartView::PreferredSize() {
	float h = 30 + fStats.size() * 28 + 50;
	return BSize(600, std::max(h, 200.0f));
}

// ---- TelemetryDialog ----

TelemetryDialog::TelemetryDialog()
	: BWindow(BRect(0, 0, 650, 400), "Usage statistics",
	          B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS) {
	auto* chart = new ChartView();
	chart->SetData(TelemetryLoad());

	auto* title = new BStringView("title", "Token usage by model");
	BFont bold = *be_bold_font;
	bold.SetSize(13);
	title->SetFont(&bold);

	auto* scroll = new BScrollView("scroll", chart,
		B_WILL_DRAW, false, true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(12)
		.Add(title)
		.Add(scroll, 1.0f);

	CenterOnScreen();
}

}  // namespace gui
}  // namespace pluto
