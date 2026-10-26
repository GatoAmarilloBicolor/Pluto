// ChatColors.cpp — Native Haiku color palette implementation.

#include "ChatColors.h"

#include <stdio.h>

namespace pluto {
namespace gui {

const char* kArrowOpen = "\xE2\x96\xBC";  // ▼
const char* kArrowClose = "\xE2\x96\xB6"; // ▶
const char* kCursorChar = "\xE2\x96\x88"; // █
const int32 kUtf3 = 3;

NativePalette gPalette;

void InitNativePalette() {
	// Backgrounds — use Haiku system colors for true dark/light support.
	gPalette.window_bg  = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	gPalette.panel_bg   = ui_color(B_PANEL_BACKGROUND_COLOR);
	gPalette.control_bg = ui_color(B_CONTROL_BACKGROUND_COLOR);
	gPalette.list_bg    = ui_color(B_LIST_BACKGROUND_COLOR);
	gPalette.list_sel_bg = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);

	// Header background: slightly lighter than window bg.
	gPalette.header_bg = Lighten(gPalette.window_bg, 8);

	// Text.
	gPalette.text_fg = ui_color(B_DOCUMENT_TEXT_COLOR);
	gPalette.text_dim = Darken(gPalette.text_fg, 60);
	gPalette.text_inv = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);

	// Link color — use system link blue if available, else custom.
	gPalette.link_color = { 0x00, 0x66, 0xCC, 0xFF };

	// Borders.
	gPalette.border = ui_color(B_PANEL_TEXT_COLOR);
	gPalette.focus_border = ui_color(B_KEYBOARD_NAVIGATION_COLOR);

	// Status / accent colors.
	gPalette.accent = ui_color(B_LINK_TEXT_COLOR);
	gPalette.success = { 0x22, 0xC5, 0x5E, 0xFF };  // green
	gPalette.warning = { 0xF5, 0x9E, 0x0B, 0xFF };  // amber
	gPalette.error   = { 0xEF, 0x44, 0x44, 0xFF };  // red
	gPalette.info    = { 0x3B, 0x82, 0xF6, 0xFF };  // blue
}

rgb_color Lighten(rgb_color c, int by) {
	auto clamp = [](int v) -> uint8 {
		return static_cast<uint8>(v < 0 ? 0 : (v > 255 ? 255 : v));
	};
	return rgb_color{
		clamp(c.red + by), clamp(c.green + by), clamp(c.blue + by), c.alpha
	};
}

rgb_color Darken(rgb_color c, int by) {
	auto clamp = [](int v) -> uint8 {
		return static_cast<uint8>(v < 0 ? 0 : (v > 255 ? 255 : v));
	};
	return rgb_color{
		clamp(c.red - by), clamp(c.green - by), clamp(c.blue - by), c.alpha
	};
}

}  // namespace gui
}  // namespace pluto
